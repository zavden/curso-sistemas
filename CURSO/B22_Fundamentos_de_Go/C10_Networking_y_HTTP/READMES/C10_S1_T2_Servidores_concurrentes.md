# Servidores concurrentes — goroutine por conexion, patrones de timeout, graceful shutdown

## 1. Introduccion

En T01 vimos servidores TCP basicos donde cada conexion se maneja en una goroutine con `go handleConn(conn)`. Ese patron funciona, pero un servidor de produccion necesita mucho mas: limites de conexiones, timeouts en multiples niveles, graceful shutdown que no corte requests en curso, health checks, metricas, y manejo correcto de errores parciales. Este topico cubre todos esos patrones.

```
┌───────────────────────────────────────────────────────────────────────────────────┐
│                    SERVIDOR TCP DE PRODUCCION — COMPONENTES                      │
├───────────────────────────────────────────────────────────────────────────────────┤
│                                                                                   │
│  ┌─────────────────────────────────────────────────────────────────┐              │
│  │                    ACCEPT LOOP                                  │              │
│  │  for {                                                          │              │
│  │      conn, err := listener.Accept()                            │              │
│  │      → Rate limiting (max conexiones simultaneas)              │              │
│  │      → Per-IP limiting (max conexiones por IP)                 │              │
│  │      → Accept deadline (para poder salir del loop)             │              │
│  │      go handleConn(conn)                                       │              │
│  │  }                                                              │              │
│  └──────────────────────────┬──────────────────────────────────────┘              │
│                              │                                                    │
│  ┌──────────────────────────▼──────────────────────────────────────┐              │
│  │                    HANDLER (goroutine por conexion)             │              │
│  │                                                                  │              │
│  │  TIMEOUTS                                                       │              │
│  │  ├─ Connection timeout  → max duracion total de la conexion    │              │
│  │  ├─ Read timeout        → max tiempo esperando datos           │              │
│  │  ├─ Write timeout       → max tiempo enviando datos            │              │
│  │  ├─ Idle timeout        → max tiempo sin actividad             │              │
│  │  └─ Request timeout     → max tiempo procesando un request     │              │
│  │                                                                  │              │
│  │  LIFECYCLE                                                      │              │
│  │  ├─ 1. Accept           → nueva conexion del SO               │              │
│  │  ├─ 2. Handshake        → protocolo de aplicacion (si aplica) │              │
│  │  ├─ 3. Request loop     → leer request, procesar, responder   │              │
│  │  ├─ 4. Shutdown signal  → dejar de aceptar, terminar en curso │              │
│  │  └─ 5. Cleanup          → cerrar conexion, liberar recursos   │              │
│  │                                                                  │              │
│  │  CANCELACION                                                    │              │
│  │  ├─ context.Context     → propagacion de cancelacion           │              │
│  │  ├─ conn.Close()        → desbloquea Read/Write               │              │
│  │  └─ WaitGroup           → esperar que terminen las goroutines │              │
│  └─────────────────────────────────────────────────────────────────┘              │
│                                                                                   │
│  ┌─────────────────────────────────────────────────────────────────┐              │
│  │                    GRACEFUL SHUTDOWN                             │              │
│  │                                                                  │              │
│  │  1. Recibir senal (SIGINT/SIGTERM)                              │              │
│  │  2. Dejar de aceptar conexiones (listener.Close())             │              │
│  │  3. Senalizar a conexiones activas (cancelar contexto)         │              │
│  │  4. Esperar que terminen (WaitGroup con timeout)               │              │
│  │  5. Forzar cierre si excede el deadline                        │              │
│  │                                                                  │              │
│  │  ┌──────┐  signal  ┌──────────┐  wait  ┌──────────┐           │              │
│  │  │SIGINT│─────────→│ stop     │───────→│ all done │           │              │
│  │  └──────┘          │ accepting│        │ cleanup  │           │              │
│  │                    └──────────┘        └──────────┘           │              │
│  │                         │                    ▲                  │              │
│  │                         │ cancel ctx         │ wg.Done()       │              │
│  │                         ▼                    │                  │              │
│  │                    ┌──────────┐              │                  │              │
│  │                    │ finish   │──────────────┘                  │              │
│  │                    │ in-flight│                                 │              │
│  │                    └──────────┘                                 │              │
│  └─────────────────────────────────────────────────────────────────┘              │
│                                                                                   │
│  MODELOS DE CONCURRENCIA EN SERVIDORES                                           │
│  ├─ C:     fork() por conexion, thread pool, o event loop (epoll)               │
│  ├─ Java:  thread pool (Executors), virtual threads (Loom), NIO                 │
│  ├─ Rust:  async tasks (tokio::spawn), thread pool                              │
│  └─ Go:    goroutine por conexion (simple, escalable a millones)                │
│                                                                                   │
│  Go puede manejar 100K+ conexiones simultaneas en un solo proceso.              │
│  Cada goroutine usa ~2KB de stack (crece dinamicamente).                        │
│  100K goroutines ≈ 200MB de RAM solo en stacks.                                 │
└───────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Goroutine por conexion — el modelo de Go

### 2.1 Modelo basico (repaso)

```go
// Este es el patron fundamental de servidores en Go.
// Cada conexion se maneja en su propia goroutine.

func main() {
    listener, err := net.Listen("tcp", ":9000")
    if err != nil {
        log.Fatal(err)
    }
    defer listener.Close()
    
    for {
        conn, err := listener.Accept()
        if err != nil {
            log.Printf("accept: %v", err)
            continue
        }
        go handleConn(conn) // goroutine por conexion
    }
}

func handleConn(conn net.Conn) {
    defer conn.Close()
    // ... logica del handler ...
}
```

### 2.2 Por que funciona en Go (y no en otros lenguajes)

```
┌─────────────────────────────────────────────────────────────────────────┐
│              POR QUE GOROUTINE-PER-CONNECTION ESCALA                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  THREAD PER CONNECTION (C, Java pre-Loom):                             │
│  ├─ Stack: 1-8 MB por thread (fijo)                                    │
│  ├─ 10K threads = 10-80 GB de RAM solo en stacks                      │
│  ├─ Context switch: ~1-10 µs (kernel mode)                             │
│  ├─ Creacion: ~100 µs - 1 ms                                           │
│  ├─ LIMITE PRACTICO: ~10K conexiones por proceso                      │
│  └─ Solucion historica: thread pools + event loops (complejidad alta) │
│                                                                         │
│  GOROUTINE PER CONNECTION (Go):                                        │
│  ├─ Stack: 2-8 KB inicial (crece dinamicamente hasta 1 GB)            │
│  ├─ 100K goroutines = ~200-800 MB de RAM en stacks                    │
│  ├─ Context switch: ~200 ns (user mode, sin kernel)                    │
│  ├─ Creacion: ~300 ns                                                   │
│  ├─ LIMITE PRACTICO: ~1M conexiones por proceso                       │
│  ├─ I/O bloqueante en codigo, asincrono en runtime (netpoll/epoll)   │
│  └─ Codigo simple: sin callbacks, sin futures, sin state machines     │
│                                                                         │
│  ASYNC PER CONNECTION (Rust tokio):                                    │
│  ├─ Task: ~64 bytes (el future en heap)                                │
│  ├─ 1M tasks = ~64 MB                                                   │
│  ├─ Requiere async/await en todo el call stack (function coloring)    │
│  ├─ Mas eficiente en memoria que Go                                    │
│  ├─ Mas complejo de escribir (Pin, lifetimes, Send bounds)            │
│  └─ LIMITE PRACTICO: ~10M conexiones (pero raro en la practica)      │
│                                                                         │
│  EN LA PRACTICA:                                                        │
│  La mayoria de servidores tienen bottleneck en:                        │
│  - File descriptors (ulimit -n, default 1024, max ~1M)                │
│  - Memoria para buffers de aplicacion (no stacks)                     │
│  - CPU para procesamiento de requests                                  │
│  - Bandwidth de red                                                     │
│  El modelo de goroutine-per-conn es suficiente para >99% de los casos │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.3 Problemas del modelo basico

```go
// El servidor basico tiene varios problemas:

func main() {
    listener, _ := net.Listen("tcp", ":9000")
    for {
        conn, _ := listener.Accept() // PROBLEMA 1: sin limite de conexiones
        go handleConn(conn)          // PROBLEMA 2: goroutines sin control
    }
}

func handleConn(conn net.Conn) {
    defer conn.Close()
    buf := make([]byte, 4096)
    for {
        n, err := conn.Read(buf)     // PROBLEMA 3: sin timeout, bloquea forever
        if err != nil {
            return
        }
        conn.Write(buf[:n])          // PROBLEMA 4: sin write timeout
    }
}

// PROBLEMAS:
// 1. Sin limite de conexiones → un atacante puede abrir miles de conexiones
// 2. Sin tracking de goroutines → no puedes hacer graceful shutdown
// 3. Sin read timeout → clientes que nunca envian datos consumen goroutines
// 4. Sin write timeout → clientes lentos bloquean goroutines
// 5. Sin graceful shutdown → Ctrl+C mata conexiones en medio de operaciones
// 6. Sin logging → no sabes que esta pasando
// 7. Sin metricas → no sabes cuantas conexiones hay activas
```

---

## 3. Limitando conexiones

### 3.1 Semaforo con channel (max conexiones globales)

```go
// Limitar el numero total de conexiones simultaneas
// usando un channel como semaforo

type Server struct {
    listener net.Listener
    sem      chan struct{} // semaforo
    maxConns int
}

func NewServer(addr string, maxConns int) (*Server, error) {
    listener, err := net.Listen("tcp", addr)
    if err != nil {
        return nil, err
    }
    return &Server{
        listener: listener,
        sem:      make(chan struct{}, maxConns),
        maxConns: maxConns,
    }, nil
}

func (s *Server) Serve() error {
    for {
        conn, err := s.listener.Accept()
        if err != nil {
            return err
        }
        
        // Intentar adquirir slot en el semaforo
        select {
        case s.sem <- struct{}{}: // slot adquirido
            go s.handleConn(conn)
        default:
            // Semaforo lleno — rechazar conexion
            log.Printf("max connections reached (%d), rejecting %s", s.maxConns, conn.RemoteAddr())
            conn.Write([]byte("server busy, try again later\n"))
            conn.Close()
        }
    }
}

func (s *Server) handleConn(conn net.Conn) {
    defer func() {
        conn.Close()
        <-s.sem // liberar slot al terminar
    }()
    
    // ... manejar conexion ...
}
```

### 3.2 Semaforo bloqueante (backpressure)

```go
// En vez de rechazar, hacer que Accept espere hasta que haya un slot.
// Esto crea backpressure: el TCP backlog del SO retiene las conexiones.

func (s *Server) ServeWithBackpressure() error {
    for {
        // Esperar slot ANTES de aceptar
        s.sem <- struct{}{} // bloquea si el semaforo esta lleno
        
        conn, err := s.listener.Accept()
        if err != nil {
            <-s.sem // devolver slot
            return err
        }
        
        go func() {
            defer func() { <-s.sem }()
            defer conn.Close()
            handleConn(conn)
        }()
    }
}

// EFECTO: cuando se llena el semaforo, Accept no se llama.
// Las conexiones pendientes se acumulan en el TCP backlog del SO.
// El SO tiene un limite (tipicamente 128-4096, configurable con net.Listen backlog).
// Si el backlog se llena, el SO rechaza nuevas conexiones (SYN → RST).
// El cliente ve "connection refused" inmediatamente.
```

### 3.3 Limite por IP

```go
// Limitar conexiones por IP para prevenir que un solo cliente
// acapare todos los slots.

type PerIPLimiter struct {
    mu       sync.Mutex
    counts   map[string]int
    maxPerIP int
}

func NewPerIPLimiter(maxPerIP int) *PerIPLimiter {
    return &PerIPLimiter{
        counts:   make(map[string]int),
        maxPerIP: maxPerIP,
    }
}

func (l *PerIPLimiter) Acquire(addr net.Addr) bool {
    ip := extractIP(addr)
    
    l.mu.Lock()
    defer l.mu.Unlock()
    
    if l.counts[ip] >= l.maxPerIP {
        return false
    }
    l.counts[ip]++
    return true
}

func (l *PerIPLimiter) Release(addr net.Addr) {
    ip := extractIP(addr)
    
    l.mu.Lock()
    defer l.mu.Unlock()
    
    l.counts[ip]--
    if l.counts[ip] <= 0 {
        delete(l.counts, ip)
    }
}

func extractIP(addr net.Addr) string {
    host, _, err := net.SplitHostPort(addr.String())
    if err != nil {
        return addr.String()
    }
    return host
}

// Uso:
func (s *Server) Serve() {
    globalSem := make(chan struct{}, 1000)  // max 1000 total
    ipLimiter := NewPerIPLimiter(10)        // max 10 por IP
    
    for {
        conn, err := s.listener.Accept()
        if err != nil {
            return
        }
        
        // Check per-IP limit
        if !ipLimiter.Acquire(conn.RemoteAddr()) {
            log.Printf("per-IP limit exceeded for %s", conn.RemoteAddr())
            conn.Close()
            continue
        }
        
        // Check global limit
        select {
        case globalSem <- struct{}{}:
            go func() {
                defer func() {
                    <-globalSem
                    ipLimiter.Release(conn.RemoteAddr())
                }()
                defer conn.Close()
                handleConn(conn)
            }()
        default:
            ipLimiter.Release(conn.RemoteAddr())
            conn.Close()
        }
    }
}
```

---

## 4. Timeouts — los cinco niveles

Los timeouts son criticos en servidores de red. Un servidor sin timeouts es vulnerable a conexiones zombi, slowloris attacks, y resource exhaustion.

### 4.1 Mapa de timeouts

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    TIMEOUTS EN UN SERVIDOR TCP                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Timeline de una conexion:                                              │
│                                                                         │
│  ──┬──────────────┬────────────┬───────────┬──────────┬──────────────  │
│    │              │            │           │          │                 │
│    ▼              ▼            ▼           ▼          ▼                 │
│  Accept      Read req     Process     Write resp   Idle...            │
│    │              │            │           │          │                 │
│    │◄──────────────────────────────────────────────────►│              │
│    │         Connection Timeout (max vida total)        │              │
│    │              │                                     │              │
│    │              │◄──────────►│                        │              │
│    │              │ Read Timeout                        │              │
│    │              │            │                        │              │
│    │              │            │◄─────────►│           │              │
│    │              │            │ Request Timeout        │              │
│    │              │            │           │            │              │
│    │              │            │           │◄────────►│ │              │
│    │              │            │           │Write Tmout │              │
│    │              │            │           │            │              │
│    │              │            │           │            │◄──────────►│ │
│    │              │            │           │            │ Idle Timeout│ │
│                                                                         │
│  1. CONNECTION TIMEOUT: duracion maxima total de la conexion.          │
│     Protege contra: clientes que mantienen conexiones forever.          │
│     Tipico: 5-30 minutos (depende del protocolo).                      │
│                                                                         │
│  2. READ TIMEOUT: tiempo maximo esperando datos del cliente.           │
│     Protege contra: slowloris (enviar datos muy lentamente),           │
│     clientes que conectan pero nunca envian.                            │
│     Tipico: 5-30 segundos.                                              │
│     Se RENUEVA en cada iteracion del request loop.                     │
│                                                                         │
│  3. WRITE TIMEOUT: tiempo maximo enviando datos al cliente.            │
│     Protege contra: clientes que no leen (buffer lleno),               │
│     conexiones donde el otro lado murio sin RST.                       │
│     Tipico: 10-30 segundos.                                             │
│                                                                         │
│  4. REQUEST TIMEOUT (via context): tiempo maximo para PROCESAR.        │
│     Protege contra: queries lentas, servicios downstream caidos.       │
│     Tipico: 5-60 segundos (depende de la operacion).                   │
│                                                                         │
│  5. IDLE TIMEOUT: tiempo maximo sin actividad entre requests.          │
│     Protege contra: conexiones keepalive que nadie usa.                │
│     Tipico: 60-120 segundos.                                            │
│     Solo aplica en protocolos con multiples requests por conexion.     │
└─────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Implementar read timeout

```go
func handleConn(conn net.Conn) {
    defer conn.Close()
    
    scanner := bufio.NewScanner(conn)
    
    for {
        // RENOVAR el deadline antes de cada lectura
        conn.SetReadDeadline(time.Now().Add(30 * time.Second))
        
        if !scanner.Scan() {
            err := scanner.Err()
            if err != nil {
                if netErr, ok := err.(net.Error); ok && netErr.Timeout() {
                    log.Printf("%s: read timeout (30s idle)", conn.RemoteAddr())
                } else {
                    log.Printf("%s: read error: %v", conn.RemoteAddr(), err)
                }
            }
            // scanner.Err() == nil → EOF (cliente cerro conexion)
            return
        }
        
        line := scanner.Text()
        processAndRespond(conn, line)
    }
}
```

### 4.3 Implementar write timeout

```go
func writeWithTimeout(conn net.Conn, data []byte, timeout time.Duration) error {
    conn.SetWriteDeadline(time.Now().Add(timeout))
    _, err := conn.Write(data)
    if err != nil {
        if netErr, ok := err.(net.Error); ok && netErr.Timeout() {
            return fmt.Errorf("write timeout (%v): client not reading fast enough", timeout)
        }
        return fmt.Errorf("write error: %w", err)
    }
    return nil
}

// Uso:
func handleConn(conn net.Conn) {
    defer conn.Close()
    
    response := []byte("resultado largo...\n")
    
    if err := writeWithTimeout(conn, response, 10*time.Second); err != nil {
        log.Printf("%s: %v", conn.RemoteAddr(), err)
        return
    }
}
```

### 4.4 Implementar connection timeout (max lifetime)

```go
// Limitar la duracion total de una conexion

func handleConn(conn net.Conn) {
    defer conn.Close()
    
    // Connection timeout: max 5 minutos de vida total
    maxLifetime := 5 * time.Minute
    deadline := time.Now().Add(maxLifetime)
    conn.SetDeadline(deadline) // Aplica a TODAS las operaciones Read/Write
    
    scanner := bufio.NewScanner(conn)
    for scanner.Scan() {
        // No renovar el deadline de la conexion
        // Solo renovar read/write si necesitas idle timeout diferente
        processAndRespond(conn, scanner.Text())
    }
}

// Alternativa con context:
func handleConnWithContext(ctx context.Context, conn net.Conn) {
    defer conn.Close()
    
    // Crear contexto con timeout total
    ctx, cancel := context.WithTimeout(ctx, 5*time.Minute)
    defer cancel()
    
    // Goroutine que cierra la conexion cuando el contexto expira
    go func() {
        <-ctx.Done()
        conn.Close() // Desbloquea cualquier Read/Write
    }()
    
    scanner := bufio.NewScanner(conn)
    for scanner.Scan() {
        select {
        case <-ctx.Done():
            return
        default:
        }
        processAndRespond(conn, scanner.Text())
    }
}
```

### 4.5 Implementar idle timeout con renovacion

```go
// El idle timeout se renueva cada vez que hay actividad.
// Si el cliente no envia nada durante el periodo, se desconecta.

func handleConnWithIdleTimeout(conn net.Conn, idleTimeout time.Duration) {
    defer conn.Close()
    
    scanner := bufio.NewScanner(conn)
    
    for {
        // Renovar idle timeout antes de cada lectura
        conn.SetReadDeadline(time.Now().Add(idleTimeout))
        
        if !scanner.Scan() {
            if err := scanner.Err(); err != nil {
                if isTimeout(err) {
                    log.Printf("%s: idle timeout (%v)", conn.RemoteAddr(), idleTimeout)
                    // Enviar mensaje de bye antes de cerrar
                    conn.SetWriteDeadline(time.Now().Add(5 * time.Second))
                    fmt.Fprintln(conn, "connection closed: idle timeout")
                }
            }
            return
        }
        
        // Actividad detectada — el deadline ya se renovó
        line := scanner.Text()
        
        // Write timeout separado para la respuesta
        conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
        response := process(line)
        if _, err := fmt.Fprintln(conn, response); err != nil {
            log.Printf("%s: write error: %v", conn.RemoteAddr(), err)
            return
        }
    }
}

func isTimeout(err error) bool {
    var netErr net.Error
    return errors.As(err, &netErr) && netErr.Timeout()
}
```

### 4.6 Request timeout con context

```go
// Para operaciones que involucran llamadas a servicios externos,
// bases de datos, etc., usa context para timeout por request.

func handleConn(conn net.Conn) {
    defer conn.Close()
    
    scanner := bufio.NewScanner(conn)
    
    for {
        conn.SetReadDeadline(time.Now().Add(30 * time.Second))
        if !scanner.Scan() {
            return
        }
        
        request := scanner.Text()
        
        // Timeout POR REQUEST: cada request tiene su propio deadline
        ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
        
        response, err := processRequest(ctx, request)
        cancel() // SIEMPRE llamar cancel
        
        if err != nil {
            if ctx.Err() == context.DeadlineExceeded {
                fmt.Fprintln(conn, "-ERR request timeout")
            } else {
                fmt.Fprintf(conn, "-ERR %v\n", err)
            }
            continue
        }
        
        conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
        fmt.Fprintf(conn, "+OK %s\n", response)
    }
}

func processRequest(ctx context.Context, request string) (string, error) {
    // Simular operacion que respeta el contexto
    // (por ejemplo, query a base de datos)
    
    resultCh := make(chan string, 1)
    errCh := make(chan error, 1)
    
    go func() {
        // La operacion real
        result, err := queryDatabase(ctx, request)
        if err != nil {
            errCh <- err
        } else {
            resultCh <- result
        }
    }()
    
    select {
    case <-ctx.Done():
        return "", ctx.Err()
    case err := <-errCh:
        return "", err
    case result := <-resultCh:
        return result, nil
    }
}
```

### 4.7 Todos los timeouts juntos

```go
// Combinando todos los timeouts en un handler completo

type ConnConfig struct {
    MaxLifetime  time.Duration // Duracion maxima total de la conexion
    ReadTimeout  time.Duration // Timeout para cada read (idle)
    WriteTimeout time.Duration // Timeout para cada write
    RequestTimeout time.Duration // Timeout para procesar cada request
}

var defaultConfig = ConnConfig{
    MaxLifetime:    5 * time.Minute,
    ReadTimeout:    30 * time.Second,
    WriteTimeout:   10 * time.Second,
    RequestTimeout: 15 * time.Second,
}

func handleConn(conn net.Conn, cfg ConnConfig) {
    defer conn.Close()
    
    // 1. Connection timeout (max lifetime)
    connCtx, connCancel := context.WithTimeout(context.Background(), cfg.MaxLifetime)
    defer connCancel()
    
    // Cerrar conexion cuando el contexto expira
    go func() {
        <-connCtx.Done()
        conn.Close()
    }()
    
    scanner := bufio.NewScanner(conn)
    
    for {
        // 2. Read/idle timeout
        conn.SetReadDeadline(time.Now().Add(cfg.ReadTimeout))
        
        if !scanner.Scan() {
            if err := scanner.Err(); err != nil {
                if isTimeout(err) {
                    log.Printf("%s: idle timeout", conn.RemoteAddr())
                } else if connCtx.Err() != nil {
                    log.Printf("%s: max lifetime exceeded", conn.RemoteAddr())
                }
            }
            return
        }
        
        request := scanner.Text()
        
        // 3. Request timeout
        reqCtx, reqCancel := context.WithTimeout(connCtx, cfg.RequestTimeout)
        response, err := processRequest(reqCtx, request)
        reqCancel()
        
        if err != nil {
            conn.SetWriteDeadline(time.Now().Add(cfg.WriteTimeout))
            fmt.Fprintf(conn, "-ERR %v\n", err)
            
            if reqCtx.Err() == context.DeadlineExceeded {
                continue // timeout del request, pero la conexion sigue viva
            }
            continue
        }
        
        // 4. Write timeout
        conn.SetWriteDeadline(time.Now().Add(cfg.WriteTimeout))
        if _, err := fmt.Fprintf(conn, "+OK %s\n", response); err != nil {
            log.Printf("%s: write error: %v", conn.RemoteAddr(), err)
            return
        }
    }
}
```

---

## 5. Graceful shutdown

El graceful shutdown es critico para evitar cortar requests en curso, perder datos, o dejar recursos sin limpiar.

### 5.1 Los pasos del graceful shutdown

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    GRACEFUL SHUTDOWN — SECUENCIA                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  PASO 1: Recibir senal de shutdown                                      │
│  ├─ SIGINT (Ctrl+C) o SIGTERM (kill, Docker stop, Kubernetes)         │
│  ├─ Capturar con signal.NotifyContext o signal.Notify                 │
│  └─ NO es SIGKILL (kill -9) — eso no se puede interceptar            │
│                                                                         │
│  PASO 2: Dejar de aceptar conexiones nuevas                            │
│  ├─ listener.Close() — hace que Accept() retorne error                │
│  ├─ Nuevos clientes reciben "connection refused"                      │
│  └─ El load balancer deberia detectar esto y redirigir trafico       │
│                                                                         │
│  PASO 3: Senalizar a conexiones activas                                │
│  ├─ Cancelar el contexto padre (context.WithCancel)                   │
│  ├─ Cada handler detecta ctx.Done() y termina su trabajo             │
│  └─ Opcionalmente: enviar mensaje "server shutting down" a clientes  │
│                                                                         │
│  PASO 4: Esperar que terminen con deadline                             │
│  ├─ sync.WaitGroup.Wait() — esperar todas las goroutines             │
│  ├─ Pero con un DEADLINE (no esperar forever)                         │
│  ├─ Tipico: 10-30 segundos de gracia                                  │
│  └─ Docker: SIGTERM → 10s → SIGKILL (configurable con stop_grace_period)│
│                                                                         │
│  PASO 5: Forzar cierre si excede el deadline                           │
│  ├─ Cerrar conexiones que siguen activas                              │
│  ├─ Loggear que conexiones fueron forzadas a cerrar                   │
│  └─ Exit con codigo 0 (shutdown fue solicitado)                       │
│                                                                         │
│  TIMELINE:                                                              │
│  ──┬────────────┬────────────────────────────────┬──────────────────   │
│    │            │                                │                     │
│  SIGTERM   Stop Accept              Deadline    Force close           │
│    │       Close Listener           10-30s      os.Exit(1)           │
│    │       Cancel ctx                 │                               │
│    │            │◄──── Grace period ──►│                              │
│    │            │  (finish in-flight)  │                               │
└─────────────────────────────────────────────────────────────────────────┘
```

### 5.2 Implementacion basica

```go
package main

import (
    "bufio"
    "context"
    "fmt"
    "log"
    "net"
    "os"
    "os/signal"
    "strings"
    "sync"
    "syscall"
    "time"
)

func main() {
    // 1. Contexto que se cancela con SIGINT/SIGTERM
    ctx, stop := signal.NotifyContext(context.Background(),
        syscall.SIGINT, syscall.SIGTERM)
    defer stop()
    
    listener, err := net.Listen("tcp", ":9000")
    if err != nil {
        log.Fatal(err)
    }
    log.Println("Server en :9000")
    
    var wg sync.WaitGroup
    
    // 2. Accept loop
    go func() {
        for {
            conn, err := listener.Accept()
            if err != nil {
                // Accept retorna error cuando listener.Close() es llamado
                // Esto es ESPERADO durante shutdown
                select {
                case <-ctx.Done():
                    return // shutdown en progreso, salir limpio
                default:
                    log.Printf("accept: %v", err)
                    continue
                }
            }
            
            wg.Add(1)
            go func() {
                defer wg.Done()
                handleConn(ctx, conn)
            }()
        }
    }()
    
    // 3. Esperar senal de shutdown
    <-ctx.Done()
    log.Println("Shutdown iniciado...")
    
    // 4. Cerrar listener (no mas conexiones nuevas)
    listener.Close()
    
    // 5. Esperar conexiones activas con deadline
    shutdownDone := make(chan struct{})
    go func() {
        wg.Wait()
        close(shutdownDone)
    }()
    
    shutdownTimeout := 15 * time.Second
    select {
    case <-shutdownDone:
        log.Println("Todas las conexiones cerradas correctamente")
    case <-time.After(shutdownTimeout):
        log.Printf("Shutdown timeout (%v) — forzando cierre", shutdownTimeout)
    }
    
    log.Println("Server detenido")
}

func handleConn(ctx context.Context, conn net.Conn) {
    defer conn.Close()
    
    addr := conn.RemoteAddr()
    log.Printf("[+] %s conectado", addr)
    defer log.Printf("[-] %s desconectado", addr)
    
    // Cerrar conexion cuando el contexto se cancela
    go func() {
        <-ctx.Done()
        // Dar un breve momento para que el handler termine naturalmente
        time.Sleep(100 * time.Millisecond)
        conn.Close() // Desbloquea Read/Write en el handler
    }()
    
    scanner := bufio.NewScanner(conn)
    
    for {
        conn.SetReadDeadline(time.Now().Add(30 * time.Second))
        
        if !scanner.Scan() {
            return
        }
        
        line := scanner.Text()
        
        // Verificar si debemos terminar
        select {
        case <-ctx.Done():
            conn.SetWriteDeadline(time.Now().Add(5 * time.Second))
            fmt.Fprintln(conn, "server shutting down, goodbye")
            return
        default:
        }
        
        response := strings.ToUpper(line)
        conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
        fmt.Fprintln(conn, response)
    }
}
```

### 5.3 Server struct completo con graceful shutdown

```go
// Patron reutilizable para servidores TCP

type TCPServer struct {
    addr     string
    listener net.Listener
    handler  func(context.Context, net.Conn)
    
    // Control de conexiones
    maxConns  int
    sem       chan struct{}
    
    // Tracking de goroutines
    wg sync.WaitGroup
    
    // Estado
    mu       sync.Mutex
    conns    map[net.Conn]struct{} // conexiones activas
    shutdown bool
    
    // Config
    shutdownTimeout time.Duration
    
    // Metricas
    totalConns   atomic.Int64
    activeConns  atomic.Int64
    totalBytes   atomic.Int64
}

func NewTCPServer(addr string, handler func(context.Context, net.Conn), maxConns int) *TCPServer {
    return &TCPServer{
        addr:            addr,
        handler:         handler,
        maxConns:        maxConns,
        sem:             make(chan struct{}, maxConns),
        conns:           make(map[net.Conn]struct{}),
        shutdownTimeout: 15 * time.Second,
    }
}

func (s *TCPServer) ListenAndServe(ctx context.Context) error {
    var err error
    s.listener, err = net.Listen("tcp", s.addr)
    if err != nil {
        return fmt.Errorf("listen: %w", err)
    }
    
    log.Printf("TCP server listening on %s (max %d conns)", s.listener.Addr(), s.maxConns)
    
    // Accept loop
    go s.acceptLoop(ctx)
    
    // Esperar cancelacion
    <-ctx.Done()
    
    return s.gracefulShutdown()
}

func (s *TCPServer) acceptLoop(ctx context.Context) {
    for {
        conn, err := s.listener.Accept()
        if err != nil {
            select {
            case <-ctx.Done():
                return
            default:
                log.Printf("accept: %v", err)
                continue
            }
        }
        
        // Verificar limite de conexiones
        select {
        case s.sem <- struct{}{}:
            // Slot adquirido
        default:
            log.Printf("max connections reached, rejecting %s", conn.RemoteAddr())
            conn.Close()
            continue
        }
        
        // Registrar conexion
        s.mu.Lock()
        if s.shutdown {
            s.mu.Unlock()
            <-s.sem
            conn.Close()
            return
        }
        s.conns[conn] = struct{}{}
        s.mu.Unlock()
        
        s.totalConns.Add(1)
        s.activeConns.Add(1)
        
        s.wg.Add(1)
        go func() {
            defer func() {
                conn.Close()
                
                s.mu.Lock()
                delete(s.conns, conn)
                s.mu.Unlock()
                
                s.activeConns.Add(-1)
                <-s.sem
                s.wg.Done()
            }()
            
            s.handler(ctx, conn)
        }()
    }
}

func (s *TCPServer) gracefulShutdown() error {
    log.Println("Initiating graceful shutdown...")
    
    // 1. Marcar como shutdown
    s.mu.Lock()
    s.shutdown = true
    s.mu.Unlock()
    
    // 2. Cerrar listener (no mas accept)
    s.listener.Close()
    
    // 3. Esperar con timeout
    done := make(chan struct{})
    go func() {
        s.wg.Wait()
        close(done)
    }()
    
    select {
    case <-done:
        log.Printf("Graceful shutdown complete (all %d connections closed)",
            s.totalConns.Load())
        return nil
        
    case <-time.After(s.shutdownTimeout):
        // 4. Forzar cierre de conexiones restantes
        s.mu.Lock()
        remaining := len(s.conns)
        for conn := range s.conns {
            conn.Close()
        }
        s.mu.Unlock()
        
        log.Printf("Forced shutdown: %d connections terminated", remaining)
        
        // Esperar un momento para que las goroutines terminen
        select {
        case <-done:
        case <-time.After(2 * time.Second):
        }
        
        return fmt.Errorf("shutdown timeout: %d connections force-closed", remaining)
    }
}

// Metricas
func (s *TCPServer) ActiveConns() int64 {
    return s.activeConns.Load()
}

func (s *TCPServer) TotalConns() int64 {
    return s.totalConns.Load()
}
```

### 5.4 Usar el TCPServer

```go
func main() {
    ctx, stop := signal.NotifyContext(context.Background(),
        syscall.SIGINT, syscall.SIGTERM)
    defer stop()
    
    server := NewTCPServer(":9000", echoHandler, 1000)
    
    if err := server.ListenAndServe(ctx); err != nil {
        log.Printf("server: %v", err)
    }
}

func echoHandler(ctx context.Context, conn net.Conn) {
    log.Printf("[+] %s", conn.RemoteAddr())
    defer log.Printf("[-] %s", conn.RemoteAddr())
    
    scanner := bufio.NewScanner(conn)
    
    for {
        conn.SetReadDeadline(time.Now().Add(30 * time.Second))
        
        if !scanner.Scan() {
            return
        }
        
        select {
        case <-ctx.Done():
            fmt.Fprintln(conn, "server shutting down")
            return
        default:
        }
        
        line := scanner.Text()
        conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
        fmt.Fprintln(conn, strings.ToUpper(line))
    }
}
```

---

## 6. Patrones de concurrencia en servidores

### 6.1 Worker pool

```go
// En vez de goroutine-per-connection, usar un pool fijo de workers.
// Util cuando quieres controlar exactamente cuantos workers hay.

type WorkerPool struct {
    jobs    chan net.Conn
    workers int
    wg      sync.WaitGroup
    handler func(net.Conn)
}

func NewWorkerPool(workers int, handler func(net.Conn)) *WorkerPool {
    wp := &WorkerPool{
        jobs:    make(chan net.Conn, workers*2), // buffer para picos
        workers: workers,
        handler: handler,
    }
    
    // Iniciar workers
    for i := 0; i < workers; i++ {
        wp.wg.Add(1)
        go wp.worker(i)
    }
    
    return wp
}

func (wp *WorkerPool) worker(id int) {
    defer wp.wg.Done()
    
    for conn := range wp.jobs {
        log.Printf("worker %d: handling %s", id, conn.RemoteAddr())
        wp.handler(conn)
    }
    log.Printf("worker %d: shutting down", id)
}

func (wp *WorkerPool) Submit(conn net.Conn) bool {
    select {
    case wp.jobs <- conn:
        return true
    default:
        return false // pool lleno
    }
}

func (wp *WorkerPool) Shutdown() {
    close(wp.jobs)
    wp.wg.Wait()
}

// Uso:
func main() {
    listener, _ := net.Listen("tcp", ":9000")
    
    pool := NewWorkerPool(50, func(conn net.Conn) {
        defer conn.Close()
        // ... handler ...
    })
    defer pool.Shutdown()
    
    for {
        conn, err := listener.Accept()
        if err != nil {
            break
        }
        if !pool.Submit(conn) {
            conn.Write([]byte("server busy\n"))
            conn.Close()
        }
    }
}
```

### 6.2 Connection multiplexer

```go
// Manejar multiples protocolos en el mismo puerto.
// Inspeccionar los primeros bytes para determinar el protocolo.

type ConnMux struct {
    listener net.Listener
    matchers []matcher
    fallback func(net.Conn)
}

type matcher struct {
    test    func([]byte) bool
    handler func(net.Conn)
}

func NewConnMux(listener net.Listener) *ConnMux {
    return &ConnMux{listener: listener}
}

// Match registra un handler que se activa si los primeros bytes coinciden
func (m *ConnMux) Match(test func([]byte) bool, handler func(net.Conn)) {
    m.matchers = append(m.matchers, matcher{test: test, handler: handler})
}

func (m *ConnMux) HandleFallback(handler func(net.Conn)) {
    m.fallback = handler
}

func (m *ConnMux) Serve() error {
    for {
        conn, err := m.listener.Accept()
        if err != nil {
            return err
        }
        go m.dispatch(conn)
    }
}

func (m *ConnMux) dispatch(conn net.Conn) {
    // Leer los primeros bytes sin consumirlos
    buf := make([]byte, 8)
    conn.SetReadDeadline(time.Now().Add(5 * time.Second))
    n, err := conn.Read(buf)
    conn.SetReadDeadline(time.Time{})
    
    if err != nil {
        conn.Close()
        return
    }
    
    peeked := buf[:n]
    
    // Crear un conn que "devuelve" los bytes leidos
    combined := &prefixConn{
        Conn:   conn,
        prefix: peeked,
    }
    
    for _, matcher := range m.matchers {
        if matcher.test(peeked) {
            matcher.handler(combined)
            return
        }
    }
    
    if m.fallback != nil {
        m.fallback(combined)
    } else {
        conn.Close()
    }
}

// prefixConn "devuelve" bytes ya leidos
type prefixConn struct {
    net.Conn
    prefix []byte
    offset int
}

func (c *prefixConn) Read(b []byte) (int, error) {
    if c.offset < len(c.prefix) {
        n := copy(b, c.prefix[c.offset:])
        c.offset += n
        return n, nil
    }
    return c.Conn.Read(b)
}

// Uso:
func main() {
    listener, _ := net.Listen("tcp", ":9000")
    
    mux := NewConnMux(listener)
    
    // HTTP: empieza con GET, POST, PUT, DELETE, etc.
    mux.Match(func(b []byte) bool {
        s := string(b)
        return strings.HasPrefix(s, "GET ") || strings.HasPrefix(s, "POST ") ||
               strings.HasPrefix(s, "PUT ") || strings.HasPrefix(s, "HEAD ")
    }, handleHTTP)
    
    // TLS: empieza con 0x16 (handshake) 0x03 (SSL/TLS)
    mux.Match(func(b []byte) bool {
        return len(b) >= 2 && b[0] == 0x16 && b[1] == 0x03
    }, handleTLS)
    
    // SSH: empieza con "SSH-"
    mux.Match(func(b []byte) bool {
        return strings.HasPrefix(string(b), "SSH-")
    }, handleSSH)
    
    // Fallback: protocolo custom
    mux.HandleFallback(handleCustom)
    
    log.Fatal(mux.Serve())
}
```

### 6.3 Pipeline de procesamiento

```go
// Patron pipeline: procesar requests en etapas con goroutines.
// Util cuando el procesamiento tiene multiples fases independientes.

func handleConn(ctx context.Context, conn net.Conn) {
    defer conn.Close()
    
    // Stage 1: Parse requests
    requests := make(chan Request, 10)
    go func() {
        defer close(requests)
        scanner := bufio.NewScanner(conn)
        for scanner.Scan() {
            select {
            case <-ctx.Done():
                return
            default:
            }
            
            req, err := parseRequest(scanner.Text())
            if err != nil {
                continue
            }
            requests <- req
        }
    }()
    
    // Stage 2: Process requests
    results := make(chan Response, 10)
    go func() {
        defer close(results)
        for req := range requests {
            resp := processRequest(ctx, req)
            results <- resp
        }
    }()
    
    // Stage 3: Send responses
    writer := bufio.NewWriter(conn)
    for resp := range results {
        conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
        data, _ := json.Marshal(resp)
        writer.Write(data)
        writer.WriteByte('\n')
        writer.Flush()
    }
}
```

---

## 7. Health checks y readiness

### 7.1 Health check TCP

```go
// Muchos load balancers verifican la salud del servidor
// intentando conectarse a un puerto TCP.
// Si la conexion se acepta, el servidor esta "sano".

// Para un health check mas sofisticado:

type HealthChecker struct {
    mu     sync.RWMutex
    ready  bool
    checks map[string]func() error
}

func NewHealthChecker() *HealthChecker {
    return &HealthChecker{
        checks: make(map[string]func() error),
    }
}

func (h *HealthChecker) RegisterCheck(name string, check func() error) {
    h.mu.Lock()
    defer h.mu.Unlock()
    h.checks[name] = check
}

func (h *HealthChecker) SetReady(ready bool) {
    h.mu.Lock()
    defer h.mu.Unlock()
    h.ready = ready
}

func (h *HealthChecker) IsHealthy() (bool, map[string]string) {
    h.mu.RLock()
    defer h.mu.RUnlock()
    
    results := make(map[string]string)
    allOK := true
    
    for name, check := range h.checks {
        if err := check(); err != nil {
            results[name] = fmt.Sprintf("FAIL: %v", err)
            allOK = false
        } else {
            results[name] = "OK"
        }
    }
    
    return allOK, results
}

func (h *HealthChecker) ServeHealthCheck(addr string) {
    listener, err := net.Listen("tcp", addr)
    if err != nil {
        log.Printf("health check listen: %v", err)
        return
    }
    defer listener.Close()
    
    for {
        conn, err := listener.Accept()
        if err != nil {
            return
        }
        
        go func() {
            defer conn.Close()
            conn.SetDeadline(time.Now().Add(5 * time.Second))
            
            healthy, results := h.IsHealthy()
            
            status := "HEALTHY"
            if !healthy {
                status = "UNHEALTHY"
            }
            
            fmt.Fprintf(conn, "%s\n", status)
            for name, result := range results {
                fmt.Fprintf(conn, "  %s: %s\n", name, result)
            }
        }()
    }
}

// Uso:
func main() {
    health := NewHealthChecker()
    
    // Registrar checks
    health.RegisterCheck("database", func() error {
        // Intentar ping a la BD
        return db.Ping()
    })
    
    health.RegisterCheck("disk", func() error {
        // Verificar espacio en disco
        var stat syscall.Statfs_t
        syscall.Statfs("/data", &stat)
        freeGB := float64(stat.Bavail*uint64(stat.Bsize)) / 1e9
        if freeGB < 1 {
            return fmt.Errorf("only %.2f GB free", freeGB)
        }
        return nil
    })
    
    // Health check en un puerto separado
    go health.ServeHealthCheck(":9001")
    
    // Servidor principal
    // ...
}
```

---

## 8. Metricas y observabilidad

### 8.1 Metricas basicas del servidor

```go
// Contadores atomicos para metricas en tiempo real

type ServerMetrics struct {
    // Conexiones
    TotalAccepted  atomic.Int64 // Total de conexiones aceptadas
    TotalRejected  atomic.Int64 // Total de rechazadas (por limites)
    ActiveConns    atomic.Int64 // Conexiones activas ahora
    PeakConns      atomic.Int64 // Maximo historico de conexiones
    
    // Bytes
    BytesRead      atomic.Int64
    BytesWritten   atomic.Int64
    
    // Errores
    ReadErrors     atomic.Int64
    WriteErrors    atomic.Int64
    TimeoutErrors  atomic.Int64
    
    // Requests
    TotalRequests  atomic.Int64
    
    // Latencia (simplificado — en produccion usar histograma)
    TotalLatencyNs atomic.Int64
    
    // Tiempo
    StartTime      time.Time
}

func NewServerMetrics() *ServerMetrics {
    return &ServerMetrics{
        StartTime: time.Now(),
    }
}

func (m *ServerMetrics) OnAccept() {
    m.TotalAccepted.Add(1)
    active := m.ActiveConns.Add(1)
    
    // Actualizar peak (compare-and-swap loop)
    for {
        peak := m.PeakConns.Load()
        if active <= peak {
            break
        }
        if m.PeakConns.CompareAndSwap(peak, active) {
            break
        }
    }
}

func (m *ServerMetrics) OnClose() {
    m.ActiveConns.Add(-1)
}

func (m *ServerMetrics) OnRequest(latency time.Duration) {
    m.TotalRequests.Add(1)
    m.TotalLatencyNs.Add(int64(latency))
}

func (m *ServerMetrics) AvgLatency() time.Duration {
    total := m.TotalRequests.Load()
    if total == 0 {
        return 0
    }
    return time.Duration(m.TotalLatencyNs.Load() / total)
}

func (m *ServerMetrics) Report() string {
    uptime := time.Since(m.StartTime).Truncate(time.Second)
    
    return fmt.Sprintf(
        "uptime=%s accepted=%d rejected=%d active=%d peak=%d "+
            "read=%s written=%s requests=%d avg_latency=%v "+
            "read_errors=%d write_errors=%d timeouts=%d",
        uptime,
        m.TotalAccepted.Load(),
        m.TotalRejected.Load(),
        m.ActiveConns.Load(),
        m.PeakConns.Load(),
        formatBytes(m.BytesRead.Load()),
        formatBytes(m.BytesWritten.Load()),
        m.TotalRequests.Load(),
        m.AvgLatency(),
        m.ReadErrors.Load(),
        m.WriteErrors.Load(),
        m.TimeoutErrors.Load(),
    )
}

// Metered connection wrapper — cuenta bytes automaticamente
type meteredConn struct {
    net.Conn
    metrics *ServerMetrics
}

func (c *meteredConn) Read(b []byte) (int, error) {
    n, err := c.Conn.Read(b)
    if n > 0 {
        c.metrics.BytesRead.Add(int64(n))
    }
    if err != nil && err != io.EOF {
        if isTimeout(err) {
            c.metrics.TimeoutErrors.Add(1)
        } else {
            c.metrics.ReadErrors.Add(1)
        }
    }
    return n, err
}

func (c *meteredConn) Write(b []byte) (int, error) {
    n, err := c.Conn.Write(b)
    if n > 0 {
        c.metrics.BytesWritten.Add(int64(n))
    }
    if err != nil {
        if isTimeout(err) {
            c.metrics.TimeoutErrors.Add(1)
        } else {
            c.metrics.WriteErrors.Add(1)
        }
    }
    return n, err
}

func formatBytes(b int64) string {
    const (
        KB = 1024
        MB = KB * 1024
        GB = MB * 1024
    )
    switch {
    case b >= GB:
        return fmt.Sprintf("%.2fGB", float64(b)/float64(GB))
    case b >= MB:
        return fmt.Sprintf("%.2fMB", float64(b)/float64(MB))
    case b >= KB:
        return fmt.Sprintf("%.2fKB", float64(b)/float64(KB))
    default:
        return fmt.Sprintf("%dB", b)
    }
}
```

### 8.2 Endpoint de metricas

```go
// Exponer metricas en un puerto separado para monitoring

func (m *ServerMetrics) ServeMetrics(addr string) {
    listener, _ := net.Listen("tcp", addr)
    defer listener.Close()
    
    for {
        conn, err := listener.Accept()
        if err != nil {
            return
        }
        go func() {
            defer conn.Close()
            conn.SetDeadline(time.Now().Add(5 * time.Second))
            fmt.Fprintln(conn, m.Report())
        }()
    }
}

// Uso: echo "" | nc localhost 9001
// Output:
// uptime=2h15m accepted=45023 rejected=12 active=342 peak=891 ...

// Loggear metricas periodicamente
func (m *ServerMetrics) LogPeriodically(interval time.Duration) {
    ticker := time.NewTicker(interval)
    defer ticker.Stop()
    
    for range ticker.C {
        log.Printf("[METRICS] %s", m.Report())
    }
}
```

---

## 9. Patron: servidor completo con todas las piezas

### 9.1 Servidor TCP de produccion

```go
package main

import (
    "bufio"
    "context"
    "encoding/json"
    "errors"
    "fmt"
    "io"
    "log"
    "net"
    "os"
    "os/signal"
    "strings"
    "sync"
    "sync/atomic"
    "syscall"
    "time"
)

// --- Configuracion ---

type Config struct {
    Addr             string        `json:"addr"`
    MaxConnections   int           `json:"max_connections"`
    MaxPerIP         int           `json:"max_per_ip"`
    ReadTimeout      time.Duration `json:"read_timeout"`
    WriteTimeout     time.Duration `json:"write_timeout"`
    IdleTimeout      time.Duration `json:"idle_timeout"`
    MaxLifetime      time.Duration `json:"max_lifetime"`
    ShutdownTimeout  time.Duration `json:"shutdown_timeout"`
    MetricsAddr      string        `json:"metrics_addr"`
}

var defaultConfig = Config{
    Addr:            ":9000",
    MaxConnections:  1000,
    MaxPerIP:        20,
    ReadTimeout:     30 * time.Second,
    WriteTimeout:    10 * time.Second,
    IdleTimeout:     60 * time.Second,
    MaxLifetime:     10 * time.Minute,
    ShutdownTimeout: 15 * time.Second,
    MetricsAddr:     ":9001",
}

// --- Metricas ---

type Metrics struct {
    Accepted  atomic.Int64
    Rejected  atomic.Int64
    Active    atomic.Int64
    Peak      atomic.Int64
    Requests  atomic.Int64
    BytesIn   atomic.Int64
    BytesOut  atomic.Int64
    Errors    atomic.Int64
    StartTime time.Time
}

func (m *Metrics) OnConnect() {
    m.Accepted.Add(1)
    active := m.Active.Add(1)
    for {
        peak := m.Peak.Load()
        if active <= peak || m.Peak.CompareAndSwap(peak, active) {
            break
        }
    }
}

func (m *Metrics) OnDisconnect()   { m.Active.Add(-1) }
func (m *Metrics) OnReject()       { m.Rejected.Add(1) }
func (m *Metrics) OnRequest()      { m.Requests.Add(1) }
func (m *Metrics) OnError()        { m.Errors.Add(1) }

func (m *Metrics) String() string {
    return fmt.Sprintf(
        "uptime=%v active=%d peak=%d total=%d rejected=%d requests=%d in=%dKB out=%dKB errors=%d",
        time.Since(m.StartTime).Truncate(time.Second),
        m.Active.Load(), m.Peak.Load(), m.Accepted.Load(),
        m.Rejected.Load(), m.Requests.Load(),
        m.BytesIn.Load()/1024, m.BytesOut.Load()/1024,
        m.Errors.Load(),
    )
}

// --- Per-IP limiter ---

type IPLimiter struct {
    mu    sync.Mutex
    conns map[string]int
    max   int
}

func NewIPLimiter(max int) *IPLimiter {
    return &IPLimiter{conns: make(map[string]int), max: max}
}

func (l *IPLimiter) Acquire(addr net.Addr) bool {
    ip, _, _ := net.SplitHostPort(addr.String())
    l.mu.Lock()
    defer l.mu.Unlock()
    if l.conns[ip] >= l.max {
        return false
    }
    l.conns[ip]++
    return true
}

func (l *IPLimiter) Release(addr net.Addr) {
    ip, _, _ := net.SplitHostPort(addr.String())
    l.mu.Lock()
    defer l.mu.Unlock()
    l.conns[ip]--
    if l.conns[ip] <= 0 {
        delete(l.conns, ip)
    }
}

// --- Servidor ---

type Server struct {
    cfg       Config
    listener  net.Listener
    metrics   *Metrics
    ipLimiter *IPLimiter
    sem       chan struct{}
    
    mu    sync.Mutex
    conns map[net.Conn]string // conn → client addr
    
    wg sync.WaitGroup
}

func NewServer(cfg Config) *Server {
    return &Server{
        cfg:       cfg,
        metrics:   &Metrics{StartTime: time.Now()},
        ipLimiter: NewIPLimiter(cfg.MaxPerIP),
        sem:       make(chan struct{}, cfg.MaxConnections),
        conns:     make(map[net.Conn]string),
    }
}

func (s *Server) Run(ctx context.Context) error {
    var err error
    s.listener, err = net.Listen("tcp", s.cfg.Addr)
    if err != nil {
        return fmt.Errorf("listen: %w", err)
    }
    
    log.Printf("Server started on %s (max_conns=%d, max_per_ip=%d)",
        s.listener.Addr(), s.cfg.MaxConnections, s.cfg.MaxPerIP)
    
    // Metricas en puerto separado
    if s.cfg.MetricsAddr != "" {
        go s.serveMetrics(s.cfg.MetricsAddr)
    }
    
    // Loggear metricas cada 30s
    go func() {
        ticker := time.NewTicker(30 * time.Second)
        defer ticker.Stop()
        for {
            select {
            case <-ctx.Done():
                return
            case <-ticker.C:
                log.Printf("[METRICS] %s", s.metrics)
            }
        }
    }()
    
    // Accept loop
    go s.acceptLoop(ctx)
    
    // Esperar senal de shutdown
    <-ctx.Done()
    
    return s.shutdown()
}

func (s *Server) acceptLoop(ctx context.Context) {
    for {
        conn, err := s.listener.Accept()
        if err != nil {
            select {
            case <-ctx.Done():
                return
            default:
                log.Printf("accept: %v", err)
                time.Sleep(100 * time.Millisecond) // Evitar busy loop en errores
                continue
            }
        }
        
        // Check per-IP limit
        if !s.ipLimiter.Acquire(conn.RemoteAddr()) {
            s.metrics.OnReject()
            log.Printf("per-IP limit reached for %s", conn.RemoteAddr())
            conn.Close()
            continue
        }
        
        // Check global limit
        select {
        case s.sem <- struct{}{}:
            // OK
        default:
            s.metrics.OnReject()
            s.ipLimiter.Release(conn.RemoteAddr())
            log.Printf("max connections reached, rejecting %s", conn.RemoteAddr())
            conn.Close()
            continue
        }
        
        // Registrar
        addr := conn.RemoteAddr().String()
        s.mu.Lock()
        s.conns[conn] = addr
        s.mu.Unlock()
        
        s.metrics.OnConnect()
        
        s.wg.Add(1)
        go func() {
            defer func() {
                conn.Close()
                s.mu.Lock()
                delete(s.conns, conn)
                s.mu.Unlock()
                s.ipLimiter.Release(conn.RemoteAddr())
                s.metrics.OnDisconnect()
                <-s.sem
                s.wg.Done()
            }()
            
            s.handleConn(ctx, conn)
        }()
    }
}

func (s *Server) handleConn(ctx context.Context, conn net.Conn) {
    addr := conn.RemoteAddr().String()
    log.Printf("[CONN] %s connected", addr)
    defer log.Printf("[CONN] %s disconnected", addr)
    
    // Connection timeout (max lifetime)
    connCtx, connCancel := context.WithTimeout(ctx, s.cfg.MaxLifetime)
    defer connCancel()
    
    // Cerrar conexion cuando el contexto expire
    go func() {
        <-connCtx.Done()
        conn.Close()
    }()
    
    // Enviar bienvenida
    conn.SetWriteDeadline(time.Now().Add(s.cfg.WriteTimeout))
    fmt.Fprintln(conn, "+OK KV Store v1.0 ready")
    
    scanner := bufio.NewScanner(conn)
    store := make(map[string]string) // store local por conexion (para demo)
    
    for {
        // Idle timeout (se renueva con cada request)
        conn.SetReadDeadline(time.Now().Add(s.cfg.IdleTimeout))
        
        if !scanner.Scan() {
            if err := scanner.Err(); err != nil {
                if isTimeout(err) {
                    conn.SetWriteDeadline(time.Now().Add(s.cfg.WriteTimeout))
                    fmt.Fprintln(conn, "-ERR idle timeout, closing connection")
                    log.Printf("[CONN] %s: idle timeout", addr)
                } else if !errors.Is(err, net.ErrClosed) {
                    s.metrics.OnError()
                    log.Printf("[CONN] %s: read error: %v", addr, err)
                }
            }
            return
        }
        
        line := strings.TrimSpace(scanner.Text())
        if line == "" {
            continue
        }
        
        s.metrics.OnRequest()
        s.metrics.BytesIn.Add(int64(len(line)))
        
        start := time.Now()
        
        // Procesar comando
        response := s.processCommand(connCtx, store, line)
        
        latency := time.Since(start)
        if latency > time.Second {
            log.Printf("[SLOW] %s: %q took %v", addr, line, latency)
        }
        
        // Enviar respuesta
        conn.SetWriteDeadline(time.Now().Add(s.cfg.WriteTimeout))
        n, err := fmt.Fprintln(conn, response)
        if err != nil {
            if isTimeout(err) {
                log.Printf("[CONN] %s: write timeout", addr)
            }
            s.metrics.OnError()
            return
        }
        s.metrics.BytesOut.Add(int64(n))
    }
}

func (s *Server) processCommand(ctx context.Context, store map[string]string, line string) string {
    parts := strings.Fields(line)
    if len(parts) == 0 {
        return "-ERR empty command"
    }
    
    cmd := strings.ToUpper(parts[0])
    args := parts[1:]
    
    switch cmd {
    case "SET":
        if len(args) < 2 {
            return "-ERR SET requires key and value"
        }
        key := args[0]
        value := strings.Join(args[1:], " ")
        store[key] = value
        return "+OK"
        
    case "GET":
        if len(args) != 1 {
            return "-ERR GET requires exactly one key"
        }
        val, ok := store[args[0]]
        if !ok {
            return "-ERR key not found"
        }
        return "+OK " + val
        
    case "DEL":
        if len(args) != 1 {
            return "-ERR DEL requires exactly one key"
        }
        _, existed := store[args[0]]
        delete(store, args[0])
        if existed {
            return "+OK deleted"
        }
        return "+OK key did not exist"
        
    case "KEYS":
        keys := make([]string, 0, len(store))
        prefix := ""
        if len(args) > 0 {
            prefix = args[0]
        }
        for k := range store {
            if prefix == "" || strings.HasPrefix(k, prefix) {
                keys = append(keys, k)
            }
        }
        if len(keys) == 0 {
            return "+OK (empty)"
        }
        return "+OK " + strings.Join(keys, " ")
        
    case "COUNT":
        return fmt.Sprintf("+OK %d", len(store))
        
    case "PING":
        if len(args) > 0 {
            return "+PONG " + strings.Join(args, " ")
        }
        return "+PONG"
        
    case "INFO":
        return fmt.Sprintf("+OK %s", s.metrics)
        
    case "QUIT":
        return "+OK bye"
        
    default:
        return fmt.Sprintf("-ERR unknown command: %s", cmd)
    }
}

func (s *Server) shutdown() error {
    log.Println("Shutting down...")
    
    // Cerrar listener
    s.listener.Close()
    
    // Listar conexiones activas
    s.mu.Lock()
    active := len(s.conns)
    s.mu.Unlock()
    log.Printf("Waiting for %d active connections...", active)
    
    // Esperar con timeout
    done := make(chan struct{})
    go func() {
        s.wg.Wait()
        close(done)
    }()
    
    select {
    case <-done:
        log.Println("All connections closed gracefully")
        return nil
    case <-time.After(s.cfg.ShutdownTimeout):
        s.mu.Lock()
        remaining := len(s.conns)
        for conn, addr := range s.conns {
            log.Printf("Force-closing connection: %s", addr)
            conn.Close()
        }
        s.mu.Unlock()
        
        <-done // Esperar que las goroutines terminen
        
        log.Printf("Shutdown complete (%d force-closed)", remaining)
        return fmt.Errorf("forced shutdown: %d connections", remaining)
    }
}

func (s *Server) serveMetrics(addr string) {
    listener, err := net.Listen("tcp", addr)
    if err != nil {
        log.Printf("metrics listen: %v", err)
        return
    }
    defer listener.Close()
    log.Printf("Metrics on %s", addr)
    
    for {
        conn, err := listener.Accept()
        if err != nil {
            return
        }
        conn.SetDeadline(time.Now().Add(5 * time.Second))
        
        data := struct {
            Uptime      string `json:"uptime"`
            Active      int64  `json:"active_connections"`
            Peak        int64  `json:"peak_connections"`
            Total       int64  `json:"total_connections"`
            Rejected    int64  `json:"rejected"`
            Requests    int64  `json:"requests"`
            BytesIn     int64  `json:"bytes_in"`
            BytesOut    int64  `json:"bytes_out"`
            Errors      int64  `json:"errors"`
        }{
            Uptime:   time.Since(s.metrics.StartTime).Truncate(time.Second).String(),
            Active:   s.metrics.Active.Load(),
            Peak:     s.metrics.Peak.Load(),
            Total:    s.metrics.Accepted.Load(),
            Rejected: s.metrics.Rejected.Load(),
            Requests: s.metrics.Requests.Load(),
            BytesIn:  s.metrics.BytesIn.Load(),
            BytesOut: s.metrics.BytesOut.Load(),
            Errors:   s.metrics.Errors.Load(),
        }
        
        json.NewEncoder(conn).Encode(data)
        conn.Close()
    }
}

func isTimeout(err error) bool {
    var netErr net.Error
    return errors.As(err, &netErr) && netErr.Timeout()
}

func main() {
    ctx, stop := signal.NotifyContext(context.Background(),
        syscall.SIGINT, syscall.SIGTERM)
    defer stop()
    
    cfg := defaultConfig
    
    // Override con variables de entorno
    if addr := os.Getenv("LISTEN_ADDR"); addr != "" {
        cfg.Addr = addr
    }
    
    server := NewServer(cfg)
    
    if err := server.Run(ctx); err != nil {
        log.Printf("server: %v", err)
    }
    
    log.Printf("[FINAL] %s", server.metrics)
}
```

```
PROBAR EL PROGRAMA:

# Terminal 1 (servidor):
go run main.go

# Output:
# Server started on :9000 (max_conns=1000, max_per_ip=20)
# Metrics on :9001

# Terminal 2 (cliente):
telnet localhost 9000
# o: nc localhost 9000

+OK KV Store v1.0 ready
SET name Alice
+OK
SET city Portland
+OK
GET name
+OK Alice
KEYS
+OK name city
COUNT
+OK 2
PING hello
+PONG hello
INFO
+OK uptime=45s active=1 peak=1 total=1 rejected=0 requests=6 ...
QUIT
+OK bye

# Terminal 3 (metricas JSON):
echo "" | nc localhost 9001 | jq
{
  "uptime": "1m30s",
  "active_connections": 2,
  "peak_connections": 3,
  "total_connections": 5,
  "rejected": 0,
  "requests": 42,
  "bytes_in": 1234,
  "bytes_out": 5678,
  "errors": 0
}

# Probar graceful shutdown:
# 1. Conectar multiples clientes con nc
# 2. Enviar Ctrl+C al servidor
# Output:
#   Shutting down...
#   Waiting for 3 active connections...
#   [CONN] 127.0.0.1:45678 disconnected
#   [CONN] 127.0.0.1:45679 disconnected
#   [CONN] 127.0.0.1:45680 disconnected
#   All connections closed gracefully
#   [FINAL] uptime=5m accepted=5 ...

# Probar idle timeout (60s default):
nc localhost 9000
# No escribir nada... despues de 60s:
# -ERR idle timeout, closing connection
# (desconexion automatica)

# Probar per-IP limit (20 por IP):
for i in $(seq 1 25); do nc localhost 9000 & done
# Los primeros 20 conectan, los ultimos 5 son rechazados

CONCEPTOS DEMOSTRADOS:
  Goroutine per connection:   go handleConn(ctx, conn)
  Global connection limit:    semaforo con channel
  Per-IP connection limit:    mapa con mutex
  Read timeout (idle):        conn.SetReadDeadline() renovable
  Write timeout:              conn.SetWriteDeadline()
  Connection max lifetime:    context.WithTimeout()
  Graceful shutdown:          signal.NotifyContext → listener.Close → wg.Wait
  Forced shutdown:            timeout → cerrar conexiones forzadamente
  Metricas atomicas:          atomic.Int64 sin mutex
  Metered connections:        wrapper que cuenta bytes
  Health check:               puerto separado con JSON
  Context propagation:        ctx fluye de main → accept → handler
  Slow query logging:         medir latencia por request
  Backpressure:               rechazar cuando lleno
```

---

## 10. Comparacion con C y Rust

```
┌────────────────────────────────┬──────────────────────────┬──────────────────────────┬───────────────────────────┐
│ Aspecto                        │ C                        │ Go                       │ Rust                      │
├────────────────────────────────┼──────────────────────────┼──────────────────────────┼───────────────────────────┤
│ Concurrencia por conexion      │ fork/thread/epoll        │ goroutine                │ tokio::spawn (async task) │
│ Costo por conexion (memoria)   │ 1-8 MB (thread stack)    │ 2-8 KB (goroutine stack) │ ~64 bytes (future)        │
│ Max conexiones practicas       │ ~10K (threads)           │ ~100K-1M                 │ ~1M-10M                   │
│                                │ ~1M (epoll event loop)   │                          │                           │
│ Complejidad del codigo         │ Alta (epoll: state       │ Baja (codigo sincrono)   │ Media (async/await)       │
│                                │ machine manual)          │                          │                           │
│ Timeout de conexion            │ setsockopt SO_RCVTIMEO   │ conn.SetDeadline()       │ tokio::time::timeout()    │
│ Timeout de request             │ alarm()/timer_create()   │ context.WithTimeout()    │ tokio::time::timeout()    │
│ Graceful shutdown              │ signal handler + flag    │ signal.NotifyContext     │ tokio::signal + cancel    │
│ Esperar goroutines/tasks       │ pthread_join() manual    │ sync.WaitGroup           │ JoinSet / JoinHandle      │
│ Limitar conexiones             │ semaforo POSIX           │ channel como semaforo    │ tokio::sync::Semaphore    │
│ Rate limiting                  │ manual con tokens        │ x/time/rate o channel    │ tower::limit::RateLimit   │
│ Metricas                       │ contadores globales      │ atomic.Int64             │ AtomicU64 / metrics crate │
│ Health check                   │ puerto extra manual      │ puerto extra o /healthz  │ tower::ready (middleware) │
│ Connection pool (cliente)      │ manual con lista/mutex   │ pool en stdlib (http)    │ pool crate (deadpool, bb8)│
│ Multiplexar protocolos         │ peek()/MSG_PEEK          │ lectura + prefixConn     │ tokio peek + custom       │
│ Worker pool                    │ thread pool + queue      │ channel + N goroutines   │ rayon / tokio workers     │
│ Context/cancelacion            │ flag global / pipe       │ context.Context          │ CancellationToken/tokio   │
│ Logging                        │ syslog/fprintf           │ log/slog (stdlib)        │ tracing crate             │
│ TLS                            │ OpenSSL (complejo)       │ crypto/tls (stdlib)      │ rustls/native-tls         │
│ Deteccion de conexion muerta   │ keepalive + read = 0     │ keepalive (Dialer)       │ keepalive (socket2)       │
│ Modelo: lectura                │ read() → buffer          │ conn.Read(buf)           │ stream.read(&mut buf)     │
│ Modelo: escritura              │ write() → buffer         │ conn.Write(buf)          │ stream.write(&buf)        │
│ Modelo: I/O multiplexing      │ epoll_wait/kqueue        │ runtime (transparente)   │ mio/tokio (transparente)  │
│ Senales                        │ sigaction() handler      │ signal.Notify channel    │ signal-hook / tokio       │
│ Compilacion a binario          │ gcc → binario            │ go build → binario       │ cargo build → binario     │
│ Cross-compile                  │ cross-gcc (complejo)     │ GOOS=x GOARCH=y (facil)  │ --target (medio)          │
│ Debugging networking           │ strace/ltrace            │ GODEBUG=netdns=go+2      │ RUST_LOG=trace            │
│ Max rendimiento (requests/s)   │ ~1M+ (epoll optimizado)  │ ~300K-500K               │ ~500K-1M (tokio)          │
│ Simplicidad de implementacion  │ ★☆☆☆☆                   │ ★★★★★                    │ ★★★☆☆                     │
└────────────────────────────────┴──────────────────────────┴──────────────────────────┴───────────────────────────┘
```

### Insight: graceful shutdown en los tres lenguajes

```
// C: graceful shutdown con senales
// Requiere flag global volatile, signal handler, y logica manual

volatile sig_atomic_t running = 1;
void handler(int sig) { running = 0; }

int main() {
    signal(SIGTERM, handler);
    int listen_fd = socket(...);
    
    while (running) {
        struct pollfd pfd = {listen_fd, POLLIN, 0};
        poll(&pfd, 1, 1000); // timeout 1s para checkear 'running'
        if (!running) break;
        
        int conn_fd = accept(listen_fd, ...);
        // ... fork/thread ...
    }
    
    // Esperar hijos: while(wait(NULL) > 0);
    close(listen_fd);
}

// Go: graceful shutdown con signal + context + WaitGroup
ctx, stop := signal.NotifyContext(ctx, syscall.SIGTERM)
// ... listener + wg ...
<-ctx.Done()
listener.Close()
wg.Wait()

// Rust (tokio): graceful shutdown con signal + CancellationToken
let token = CancellationToken::new();
let listener = TcpListener::bind("0.0.0.0:9000").await?;

tokio::select! {
    _ = accept_loop(&listener, token.clone()) => {},
    _ = tokio::signal::ctrl_c() => {
        token.cancel();
        // wait for tasks...
    }
}
```

---

## 11. Antipatrones y errores comunes

### 11.1 Catálogo de errores

```go
// ERROR 1: No cerrar la conexion
func handleConn(conn net.Conn) {
    // MAL: si esta funcion retorna por un error, la conexion queda abierta
    scanner := bufio.NewScanner(conn)
    for scanner.Scan() {
        // ...
    }
    // conn.Close() nunca se llama si hay un return anticipado
}
// BIEN:
func handleConn(conn net.Conn) {
    defer conn.Close() // SIEMPRE defer Close al inicio
    // ...
}
```

```go
// ERROR 2: Ignorar errores de Accept
for {
    conn, err := listener.Accept()
    if err != nil {
        continue // MAL: puede ser un error permanente (too many open files)
    }
    go handleConn(conn)
}
// BIEN: loggear y/o detectar errores permanentes
for {
    conn, err := listener.Accept()
    if err != nil {
        if errors.Is(err, net.ErrClosed) {
            return // listener cerrado (shutdown)
        }
        log.Printf("accept: %v", err)
        // Back off para evitar busy loop en errores temporales
        time.Sleep(100 * time.Millisecond)
        continue
    }
    go handleConn(conn)
}
```

```go
// ERROR 3: Leak de goroutines — no trackear con WaitGroup
for {
    conn, _ := listener.Accept()
    go handleConn(conn) // MAL: no sabes cuando terminan
}
// BIEN:
var wg sync.WaitGroup
for {
    conn, _ := listener.Accept()
    wg.Add(1)
    go func() {
        defer wg.Done()
        handleConn(conn)
    }()
}
// Al hacer shutdown: wg.Wait()
```

```go
// ERROR 4: Read sin timeout — goroutine bloqueada forever
func handleConn(conn net.Conn) {
    defer conn.Close()
    buf := make([]byte, 1024)
    n, _ := conn.Read(buf) // MAL: bloquea indefinidamente
}
// BIEN:
conn.SetReadDeadline(time.Now().Add(30 * time.Second))
n, err := conn.Read(buf)
```

```go
// ERROR 5: Leer un byte a la vez
func handleConn(conn net.Conn) {
    defer conn.Close()
    buf := make([]byte, 1) // MAL: un syscall read() por byte
    for {
        conn.Read(buf)
        processByte(buf[0])
    }
}
// BIEN: usar bufio
scanner := bufio.NewScanner(conn) // Buffer interno de 64KB
// o
reader := bufio.NewReaderSize(conn, 8192) // Buffer de 8KB
```

```go
// ERROR 6: Asumir que Read devuelve un mensaje completo
func handleConn(conn net.Conn) {
    defer conn.Close()
    buf := make([]byte, 1024)
    n, _ := conn.Read(buf)
    
    // MAL: asumir que buf[:n] contiene un mensaje completo
    msg := string(buf[:n])
    // Puede contener medio mensaje, o dos mensajes pegados
}
// BIEN: usar un protocolo (delimitador, length-prefix, etc.)
scanner := bufio.NewScanner(conn)
for scanner.Scan() { // Lee hasta '\n' — un mensaje completo
    msg := scanner.Text()
    // ...
}
```

```go
// ERROR 7: Escribir sin verificar error
func handleConn(conn net.Conn) {
    defer conn.Close()
    conn.Write([]byte("respuesta\n")) // MAL: ignora error
    // Si el cliente cerro, Write retorna error y datos se pierden
}
// BIEN:
_, err := conn.Write([]byte("respuesta\n"))
if err != nil {
    log.Printf("write error: %v", err)
    return // conexion rota — no intentar mas writes
}
```

```go
// ERROR 8: Usar fmt.Sprintf para construir host:port
addr := fmt.Sprintf("%s:%d", host, port) // MAL: falla con IPv6
// Si host = "::1", resultado: "::1:8080" (ambiguo)

// BIEN: siempre usar JoinHostPort
addr := net.JoinHostPort(host, strconv.Itoa(port))
// Resultado: "[::1]:8080" (correcto)
```

---

## 12. Ejercicios

### Ejercicio 1: Chat server con rooms
Expande el chat server de T01 con:
- Subcomandos del chat: `/join <room>`, `/leave`, `/rooms` (listar rooms), `/who` (listar usuarios en la room actual), `/nick <nombre>` (cambiar nick), `/msg <usuario> <texto>` (mensaje privado)
- Cada room tiene su propio canal de broadcast
- Un usuario solo esta en una room a la vez (al hacer `/join` sale de la anterior)
- Implementa timeouts: idle 5 minutos desconecta, warning a los 4 minutos
- Graceful shutdown: al recibir SIGTERM, enviar "server shutting down" a todos los clientes, esperar 10s, cerrar
- Metricas: contar usuarios por room, mensajes totales, bytes transferidos
- Rate limiting: max 10 mensajes por segundo por usuario

### Ejercicio 2: TCP reverse proxy con balanceo
Crea un reverse proxy TCP que:
- Escuche en un puerto local y reenvie a N backends (configurables por CLI o archivo JSON)
- Implemente round-robin, random, o least-connections como estrategia de balanceo (flag)
- Health check de backends cada 10s (intentar TCP connect): si un backend falla, removerlo del pool; si vuelve, reincorporarlo
- Graceful shutdown: dejar de aceptar, esperar conexiones en curso
- Metricas por backend: conexiones activas, total, errores, latencia promedio
- Log de cada conexion: origen, destino elegido, bytes transferidos, duracion
- Timeout de conexion al backend configurable

### Ejercicio 3: Servidor con protocolo binario
Implementa un servidor de archivos con protocolo TLV (type-length-value):
- Tipos de mensaje: LIST (listar directorio), GET (descargar archivo), PUT (subir archivo), DELETE (borrar), STAT (info de archivo)
- Protocolo binario: 1 byte tipo + 4 bytes longitud + N bytes payload
- Payload en JSON para requests/responses (excepto datos de archivo que van raw)
- Autenticacion basica: primer mensaje debe ser AUTH con usuario:password
- Soporte de transferencia resumible: GET con offset para continuar descargas interrumpidas
- Limite de velocidad (bandwidth throttling): configurable por conexion
- Graceful shutdown con WaitGroup

### Ejercicio 4: Connection pool para clientes
Implementa un pool de conexiones TCP reutilizables:
- `Pool.Get() (net.Conn, error)` — obtener una conexion (existente o nueva)
- `Pool.Put(conn)` — devolver una conexion al pool
- Max conexiones configurable; si el pool esta lleno, `Get()` bloquea (con timeout via context)
- Health check: verificar que la conexion sigue viva antes de retornarla (Write + Read test o TCP keepalive)
- Idle timeout: cerrar conexiones que llevan mas de N segundos sin uso
- Metricas: pool size, hits (conexion reusada), misses (nueva conexion), stale (cerrada por idle)
- Thread-safe (multiple goroutines llamando Get/Put simultaneamente)

---

> **Siguiente**: C10/S02 - HTTP, T01 - net/http cliente — http.Get, http.Client, timeouts, headers, cookies
