# net package — net.Dial, net.Listen, net.Conn, TCP client/server basico

## 1. Introduccion

El paquete `net` es la base de todo networking en Go. Proporciona una interfaz portable para I/O de red incluyendo TCP/IP, UDP, DNS y Unix domain sockets. A diferencia de muchos lenguajes donde el networking requiere librerias externas o APIs complejas, Go expone primitivas simples (`Dial`, `Listen`, `Accept`) que devuelven conexiones que implementan `io.Reader` y `io.Writer` — lo que significa que todo lo aprendido sobre I/O en Go se aplica directamente al networking.

```
┌───────────────────────────────────────────────────────────────────────────────────┐
│                    net PACKAGE — ARQUITECTURA                                    │
├───────────────────────────────────────────────────────────────────────────────────┤
│                                                                                   │
│  CAPAS DE ABSTRACCION                                                            │
│                                                                                   │
│  ┌─────────────────────────────────────────────────────────────┐                 │
│  │                    APLICACION                                │                 │
│  │  net/http, net/rpc, net/smtp, database/sql, gRPC, etc.     │                 │
│  └──────────────────────────┬──────────────────────────────────┘                 │
│                              │                                                    │
│  ┌──────────────────────────▼──────────────────────────────────┐                 │
│  │                    net PACKAGE                               │                 │
│  │                                                              │                 │
│  │  INTERFACES                                                  │                 │
│  │  ├─ net.Conn          → conexion bidireccional (TCP, Unix)  │                 │
│  │  ├─ net.Listener      → acepta conexiones entrantes         │                 │
│  │  ├─ net.PacketConn    → datagramas (UDP)                    │                 │
│  │  ├─ net.Addr          → direccion de red                     │                 │
│  │  └─ net.Error         → errores de red (Timeout, Temporary) │                 │
│  │                                                              │                 │
│  │  FUNCIONES                                                   │                 │
│  │  ├─ net.Dial(network, address)    → conectar como cliente   │                 │
│  │  ├─ net.Listen(network, address)  → escuchar como servidor  │                 │
│  │  ├─ net.DialTimeout(net, addr, t) → Dial con timeout        │                 │
│  │  ├─ net.LookupHost(host)         → resolucion DNS           │                 │
│  │  ├─ net.LookupAddr(addr)         → DNS reverso              │                 │
│  │  ├─ net.InterfaceAddrs()         → IPs locales              │                 │
│  │  └─ net.JoinHostPort(host, port) → "host:port"             │                 │
│  │                                                              │                 │
│  │  TIPOS CONCRETOS                                             │                 │
│  │  ├─ *net.TCPConn      → conexion TCP                       │                 │
│  │  ├─ *net.UDPConn      → conexion UDP                       │                 │
│  │  ├─ *net.UnixConn     → conexion Unix socket               │                 │
│  │  ├─ *net.TCPListener  → listener TCP                        │                 │
│  │  ├─ *net.TCPAddr      → direccion TCP (IP + port)          │                 │
│  │  ├─ *net.UDPAddr      → direccion UDP (IP + port)          │                 │
│  │  ├─ *net.IPAddr       → direccion IP pura                  │                 │
│  │  └─ *net.Dialer       → configuracion avanzada de Dial     │                 │
│  └──────────────────────────┬──────────────────────────────────┘                 │
│                              │                                                    │
│  ┌──────────────────────────▼──────────────────────────────────┐                 │
│  │                    SISTEMA OPERATIVO                         │                 │
│  │  socket(), bind(), listen(), accept(), connect()             │                 │
│  │  read(), write(), close(), setsockopt()                      │                 │
│  │  epoll (Linux) / kqueue (macOS) / IOCP (Windows)            │                 │
│  └─────────────────────────────────────────────────────────────┘                 │
│                                                                                   │
│  MODELO I/O DE GO                                                                │
│  Go usa un modelo de I/O SINCRONO con scheduling ASINCRONO:                     │
│  - Tu codigo es bloqueante (Read/Write bloquean la goroutine)                   │
│  - El runtime usa epoll/kqueue internamente                                      │
│  - Cuando una goroutine bloquea en I/O, el runtime la "parkea"                  │
│    y usa el thread (M) para otra goroutine                                       │
│  - Resultado: codigo simple (sincrono) con rendimiento asincrono               │
│                                                                                   │
│  C:    sincrono bloqueante (un thread por conexion) o                            │
│         asincrono explicito (epoll/kqueue + event loop)                          │
│  Rust: asincrono explicito (async/await + tokio/mio)                            │
│  Go:   sincrono en el codigo, asincrono en el runtime (lo mejor de ambos)       │
│                                                                                   │
│  net.Conn IMPLEMENTA io.Reader + io.Writer                                      │
│  Esto significa que TODO lo que funciona con io.Reader/Writer                    │
│  funciona con conexiones de red:                                                 │
│  - bufio.Scanner para leer lineas de una conexion                               │
│  - io.Copy para transferir datos entre conexiones                                │
│  - json.NewDecoder(conn) para decodificar JSON de la red                        │
│  - fmt.Fprintf(conn, ...) para escribir texto formateado                        │
│  - gzip.NewReader(conn) para descomprimir on-the-fly                            │
└───────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Redes TCP vs UDP — fundamentos

Antes de ver el codigo Go, es importante entender los dos protocolos de transporte principales y cuando usar cada uno.

### 2.1 TCP (Transmission Control Protocol)

```
┌──────────────────────────────────────────────────────────────────────┐
│                    TCP — ORIENTADO A CONEXION                       │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  CARACTERISTICAS:                                                    │
│  ├─ Orientado a conexion (handshake de 3 pasos)                    │
│  ├─ Entrega GARANTIZADA y ORDENADA                                  │
│  ├─ Control de flujo (el receptor controla la velocidad)            │
│  ├─ Control de congestion (adapta velocidad a la red)               │
│  ├─ Stream de bytes (no preserva limites de mensajes)              │
│  ├─ Full-duplex (ambos lados leen y escriben simultaneamente)      │
│  └─ Shutdown graceful (FIN handshake de 4 pasos)                   │
│                                                                      │
│  HANDSHAKE TCP (3-way):                                              │
│  Cliente                          Servidor                           │
│    │                                │                                │
│    │─────── SYN ──────────────────→│  "Quiero conectar"            │
│    │                                │                                │
│    │←────── SYN+ACK ──────────────│  "OK, yo tambien"              │
│    │                                │                                │
│    │─────── ACK ──────────────────→│  "Confirmado"                 │
│    │                                │                                │
│    │←═══════ DATOS ═══════════════→│  Comunicacion bidireccional   │
│    │                                │                                │
│    │─────── FIN ──────────────────→│  "Voy a cerrar"              │
│    │←────── ACK ──────────────────│  "OK"                          │
│    │←────── FIN ──────────────────│  "Yo tambien"                  │
│    │─────── ACK ──────────────────→│  "OK"                         │
│                                                                      │
│  STREAM DE BYTES (importante!):                                      │
│  Si envias "Hello" y luego "World", el receptor puede recibir:     │
│  - "Hello" + "World"       (dos reads)                              │
│  - "HelloWorld"            (un read)                                │
│  - "Hel" + "loWor" + "ld" (tres reads)                             │
│  TCP NO preserva limites de mensajes — debes usar un protocolo     │
│  de aplicacion (delimitadores, longitud prefijada, etc.)            │
│                                                                      │
│  USOS: HTTP, HTTPS, SSH, SMTP, FTP, bases de datos, APIs           │
└──────────────────────────────────────────────────────────────────────┘
```

### 2.2 UDP (User Datagram Protocol)

```
┌──────────────────────────────────────────────────────────────────────┐
│                    UDP — DATAGRAMAS                                  │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  CARACTERISTICAS:                                                    │
│  ├─ Sin conexion (no hay handshake)                                 │
│  ├─ NO hay garantia de entrega (paquetes pueden perderse)           │
│  ├─ NO hay garantia de orden (pueden llegar desordenados)           │
│  ├─ Preserva limites de mensajes (cada send = un datagrama)        │
│  ├─ Menor overhead (no hay control de flujo/congestion)             │
│  ├─ Multicast y broadcast posibles                                  │
│  └─ MTU: ~1500 bytes por datagrama en Ethernet                     │
│                                                                      │
│  COMUNICACION:                                                       │
│  Cliente                          Servidor                           │
│    │                                │                                │
│    │─────── Datagrama 1 ─────────→│  (puede llegar)               │
│    │─────── Datagrama 2 ─────────→│  (puede perderse)             │
│    │─────── Datagrama 3 ─────────→│  (puede llegar antes que 1)   │
│    │                                │                                │
│    │←────── Respuesta ────────────│  (opcional, puede perderse)    │
│                                                                      │
│  CADA DATAGRAMA ES INDEPENDIENTE:                                    │
│  Si envias "Hello" y luego "World", el receptor recibe:            │
│  - "Hello" (un read, un datagrama)                                  │
│  - "World" (un read, un datagrama)                                  │
│  Los limites del mensaje se preservan.                               │
│                                                                      │
│  USOS: DNS, DHCP, streaming de video/audio, gaming, IoT, mDNS     │
└──────────────────────────────────────────────────────────────────────┘
```

### 2.3 Cuando usar TCP vs UDP

```
┌────────────────────────────┬──────────────────┬──────────────────┐
│ Criterio                   │ TCP              │ UDP              │
├────────────────────────────┼──────────────────┼──────────────────┤
│ Fiabilidad necesaria       │ ✓ Si             │ ✗ No critica     │
│ Orden garantizado          │ ✓ Si             │ ✗ No             │
│ Latencia critica           │ Mayor latencia   │ ✓ Minima         │
│ Multiples clientes         │ 1 conn por cli   │ ✓ 1 socket       │
│ Mensajes largos            │ ✓ Sin limite     │ ~1400 bytes      │
│ Broadcast/multicast        │ ✗ No             │ ✓ Si             │
│ Overhead por conexion      │ ~400 bytes RAM   │ Ninguno          │
│ Ejemplo tipico             │ HTTP, DB, SSH    │ DNS, video, game │
│ En Go: tipo                │ net.Conn         │ net.PacketConn   │
│ En Go: crear               │ net.Dial/Listen  │ net.ListenPacket │
└────────────────────────────┴──────────────────┴──────────────────┘
```

---

## 3. net.Conn — la interfaz central

`net.Conn` es la interfaz que representa una conexion de red genérica. Es el corazon del paquete `net`.

### 3.1 Definicion de la interfaz

```go
// net.Conn es la interfaz que toda conexion de red implementa.
// Implementa io.Reader, io.Writer, e io.Closer.

type Conn interface {
    // Read lee datos de la conexion.
    // Mismo contrato que io.Reader:
    // - Retorna n > 0 bytes leidos
    // - Retorna io.EOF cuando la conexion se cierra
    // - Puede retornar n > 0 y err != nil simultaneamente
    Read(b []byte) (n int, err error)
    
    // Write escribe datos a la conexion.
    // Mismo contrato que io.Writer:
    // - Retorna n == len(b) si exito
    // - Si n < len(b), retorna error no-nil
    Write(b []byte) (n int, err error)
    
    // Close cierra la conexion.
    // Cualquier Read o Write bloqueado se desbloquea con error.
    Close() error
    
    // LocalAddr retorna la direccion local de la conexion.
    LocalAddr() Addr
    
    // RemoteAddr retorna la direccion remota.
    RemoteAddr() Addr
    
    // SetDeadline setea deadline para Read y Write.
    // t = time.Time{} (zero value) significa "sin deadline".
    // Un deadline es un punto ABSOLUTO en el tiempo, no una duracion.
    SetDeadline(t time.Time) error
    
    // SetReadDeadline setea deadline solo para Read.
    SetReadDeadline(t time.Time) error
    
    // SetWriteDeadline setea deadline solo para Write.
    SetWriteDeadline(t time.Time) error
}
```

### 3.2 Deadlines vs timeouts

```go
// DEADLINE: punto absoluto en el tiempo
conn.SetDeadline(time.Now().Add(5 * time.Second))
// "Esta operacion debe completarse antes de las 14:30:05"

// Si el deadline pasa, Read/Write retornan un error que satisface:
//   err.(net.Error).Timeout() == true

// PATRON: renovar deadline antes de cada operacion
for {
    conn.SetReadDeadline(time.Now().Add(30 * time.Second))
    n, err := conn.Read(buf)
    if err != nil {
        if netErr, ok := err.(net.Error); ok && netErr.Timeout() {
            fmt.Println("timeout — el cliente no envio datos en 30s")
            continue // o break para desconectar
        }
        break // otro error — conexion cerrada, etc.
    }
    process(buf[:n])
}

// IMPORTANTE: un deadline NO se resetea automaticamente.
// Despues de un timeout, la conexion sigue viva.
// Debes setear un nuevo deadline antes de la siguiente operacion.

// Para quitar el deadline:
conn.SetDeadline(time.Time{}) // zero value = sin deadline
```

### 3.3 Direcciones de red (net.Addr)

```go
// net.Addr es la interfaz para direcciones de red

type Addr interface {
    Network() string // "tcp", "udp", "unix", etc.
    String() string  // representacion legible: "192.168.1.1:8080"
}

// Tipos concretos de direccion:
// *net.TCPAddr  — IP + Port
// *net.UDPAddr  — IP + Port
// *net.UnixAddr — path del socket
// *net.IPAddr   — IP sin port

// Obtener informacion de una conexion:
func handleConn(conn net.Conn) {
    local := conn.LocalAddr()   // nuestra direccion
    remote := conn.RemoteAddr() // direccion del otro lado
    
    fmt.Printf("Conexion: %s -> %s\n", remote, local)
    fmt.Printf("Network: %s\n", remote.Network()) // "tcp"
    
    // Cast a tipo concreto para acceder a IP y Port:
    if tcpAddr, ok := remote.(*net.TCPAddr); ok {
        fmt.Printf("IP: %s\n", tcpAddr.IP)
        fmt.Printf("Port: %d\n", tcpAddr.Port)
        fmt.Printf("Zone: %s\n", tcpAddr.Zone) // para IPv6 link-local
    }
}
```

### 3.4 Resolver direcciones manualmente

```go
// Normalmente Dial/Listen resuelven direcciones automaticamente.
// Pero puedes resolverlas manualmente:

// Resolver TCP address
addr, err := net.ResolveTCPAddr("tcp", "example.com:80")
// addr.IP = 93.184.216.34
// addr.Port = 80

// Resolver UDP address
udpAddr, err := net.ResolveUDPAddr("udp", "8.8.8.8:53")

// Crear direccion sin resolver
addr := &net.TCPAddr{
    IP:   net.ParseIP("127.0.0.1"),
    Port: 8080,
}

// Formatear host:port (maneja IPv6 correctamente)
hostport := net.JoinHostPort("::1", "8080")
// Resultado: "[::1]:8080" (con brackets para IPv6)

host, port, err := net.SplitHostPort("[::1]:8080")
// host = "::1", port = "8080"

// CUIDADO con IPv6:
// net.JoinHostPort("::1", "8080")  → "[::1]:8080"  ✓ correcto
// fmt.Sprintf("%s:%s", "::1", "8080") → "::1:8080" ✗ ambiguo
// SIEMPRE usar JoinHostPort para evitar problemas con IPv6
```

---

## 4. net.Dial — conectarse como cliente

`net.Dial` es la forma de conectarse a un servidor remoto. Es el equivalente de `connect()` en sockets BSD.

### 4.1 Uso basico

```go
package main

import (
    "fmt"
    "io"
    "net"
    "os"
)

func main() {
    // Conectar a un servidor TCP
    conn, err := net.Dial("tcp", "example.com:80")
    if err != nil {
        fmt.Fprintf(os.Stderr, "error: %v\n", err)
        os.Exit(1)
    }
    defer conn.Close()
    
    // Enviar una solicitud HTTP simple
    fmt.Fprintf(conn, "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n")
    
    // Leer la respuesta
    response, err := io.ReadAll(conn)
    if err != nil {
        fmt.Fprintf(os.Stderr, "read error: %v\n", err)
        os.Exit(1)
    }
    
    fmt.Println(string(response))
}
```

### 4.2 Redes soportadas por Dial

```go
// El primer argumento de Dial es la red:

// TCP (lo mas comun)
conn, err := net.Dial("tcp", "example.com:80")   // TCP IPv4 o IPv6
conn, err := net.Dial("tcp4", "example.com:80")  // Solo IPv4
conn, err := net.Dial("tcp6", "example.com:80")  // Solo IPv6

// UDP
conn, err := net.Dial("udp", "8.8.8.8:53")      // UDP IPv4 o IPv6
conn, err := net.Dial("udp4", "8.8.8.8:53")     // Solo IPv4

// Unix domain sockets (comunicacion local)
conn, err := net.Dial("unix", "/var/run/docker.sock")      // stream (como TCP)
conn, err := net.Dial("unixgram", "/tmp/my.sock")          // datagram (como UDP)
conn, err := net.Dial("unixpacket", "/tmp/my.sock")        // seqpacket

// IP raw (requiere permisos root)
conn, err := net.Dial("ip4:icmp", "example.com")  // ICMP (para ping)
conn, err := net.Dial("ip4:1", "example.com")     // protocolo por numero

// Formato de address:
// TCP/UDP: "host:port" o "[host]:port" (IPv6)
// Unix:    "/path/to/socket"
// IP:      "host"
```

### 4.3 net.DialTimeout — Dial con timeout

```go
// net.Dial puede bloquearse mucho tiempo si el servidor no responde.
// El timeout de SO es tipicamente 2+ minutos.

// Dial con timeout explicito:
conn, err := net.DialTimeout("tcp", "example.com:80", 5*time.Second)
if err != nil {
    // Puede ser timeout o error de conexion
    if netErr, ok := err.(net.Error); ok && netErr.Timeout() {
        fmt.Println("connection timed out")
    } else {
        fmt.Printf("connection error: %v\n", err)
    }
    return
}
defer conn.Close()
```

### 4.4 net.Dialer — configuracion avanzada

```go
// net.Dialer ofrece control total sobre la conexion

dialer := &net.Dialer{
    Timeout:   5 * time.Second,       // Timeout de conexion
    KeepAlive: 30 * time.Second,      // TCP keepalive interval
    // Los keepalives detectan conexiones muertas
    // (router caido, cable desconectado, etc.)
    
    LocalAddr: &net.TCPAddr{          // IP local a usar (multihomed hosts)
        IP: net.ParseIP("192.168.1.100"),
    },
    
    DualStack: true,                  // Probar IPv6 y IPv4 (obsoleto, es default)
    
    FallbackDelay: 300 * time.Millisecond, // Delay antes de fallback IPv6→IPv4
    // "Happy Eyeballs": intenta IPv6 primero, si no responde en 300ms,
    // intenta IPv4 en paralelo. Usa la primera que conecte.
    
    Resolver: &net.Resolver{          // DNS custom
        PreferGo: true,               // Usar resolver de Go (no cgo)
    },
}

// Dial con la configuracion
conn, err := dialer.Dial("tcp", "example.com:443")

// Dial con contexto (permite cancelacion)
ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
defer cancel()
conn, err := dialer.DialContext(ctx, "tcp", "example.com:443")
```

### 4.5 Errores comunes de Dial

```go
conn, err := net.Dial("tcp", "example.com:80")
if err != nil {
    // Tipos de errores que puedes recibir:
    
    var dnsErr *net.DNSError
    if errors.As(err, &dnsErr) {
        // Error de resolucion DNS
        fmt.Printf("DNS error: %v\n", dnsErr)
        fmt.Printf("  Host: %s\n", dnsErr.Name)
        fmt.Printf("  Is not found: %v\n", dnsErr.IsNotFound)
        fmt.Printf("  Is timeout: %v\n", dnsErr.IsTimeout)
        // Ejemplo: lookup nonexistent.example.com: no such host
    }
    
    var opErr *net.OpError
    if errors.As(err, &opErr) {
        // Error de operacion de red
        fmt.Printf("Op: %s\n", opErr.Op)        // "dial"
        fmt.Printf("Net: %s\n", opErr.Net)      // "tcp"
        fmt.Printf("Addr: %s\n", opErr.Addr)    // "example.com:80"
        fmt.Printf("Timeout: %v\n", opErr.Timeout())
        
        // El error subyacente puede ser:
        var sysErr *os.SyscallError
        if errors.As(opErr.Err, &sysErr) {
            fmt.Printf("Syscall: %s\n", sysErr.Syscall) // "connect"
            // errno: ECONNREFUSED (puerto no abierto)
            //        ETIMEDOUT (host no alcanzable)
            //        ENETUNREACH (red no alcanzable)
            //        ECONNRESET (conexion reseteada)
        }
    }
    
    // Forma simple: solo check timeout
    if netErr, ok := err.(net.Error); ok && netErr.Timeout() {
        fmt.Println("timeout!")
    }
    
    return
}
```

---

## 5. net.Listen — escuchar como servidor

`net.Listen` crea un listener que acepta conexiones entrantes. Es el equivalente de `socket()` + `bind()` + `listen()` en sockets BSD.

### 5.1 Uso basico

```go
// net.Listen retorna un net.Listener

type Listener interface {
    // Accept espera y retorna la siguiente conexion.
    // Bloquea hasta que llega una conexion o el listener se cierra.
    Accept() (Conn, error)
    
    // Close cierra el listener.
    // Cualquier Accept() bloqueado retorna error.
    Close() error
    
    // Addr retorna la direccion del listener.
    Addr() Addr
}
```

```go
package main

import (
    "fmt"
    "net"
    "os"
)

func main() {
    // Escuchar en todas las interfaces, puerto 8080
    listener, err := net.Listen("tcp", ":8080")
    if err != nil {
        fmt.Fprintf(os.Stderr, "listen error: %v\n", err)
        os.Exit(1)
    }
    defer listener.Close()
    
    fmt.Printf("Escuchando en %s\n", listener.Addr())
    
    // Aceptar conexiones en un loop
    for {
        conn, err := listener.Accept()
        if err != nil {
            fmt.Fprintf(os.Stderr, "accept error: %v\n", err)
            continue
        }
        
        // Manejar cada conexion
        fmt.Printf("Nueva conexion de %s\n", conn.RemoteAddr())
        conn.Write([]byte("Hola! Conexion recibida.\n"))
        conn.Close()
    }
}
```

### 5.2 Formatos de address para Listen

```go
// Escuchar en todas las interfaces
listener, _ := net.Listen("tcp", ":8080")       // 0.0.0.0:8080 + [::]:8080
listener, _ := net.Listen("tcp4", ":8080")      // Solo IPv4 0.0.0.0:8080
listener, _ := net.Listen("tcp6", ":8080")      // Solo IPv6 [::]:8080

// Escuchar en una IP especifica
listener, _ := net.Listen("tcp", "127.0.0.1:8080")  // Solo localhost
listener, _ := net.Listen("tcp", "192.168.1.10:8080") // Solo esa IP

// Puerto aleatorio (util para testing)
listener, _ := net.Listen("tcp", ":0")          // El SO asigna un puerto libre
fmt.Println(listener.Addr()) // "127.0.0.1:43567" (ejemplo)

// Obtener el puerto asignado:
addr := listener.Addr().(*net.TCPAddr)
fmt.Printf("Puerto asignado: %d\n", addr.Port)

// Unix domain socket (comunicacion local, mas rapido que TCP loopback)
listener, _ := net.Listen("unix", "/tmp/myapp.sock")
// El archivo del socket se crea automaticamente
// NOTA: debes borrar el archivo al cerrar (no se borra solo)
defer os.Remove("/tmp/myapp.sock")
```

### 5.3 net.ListenConfig — configuracion avanzada

```go
lc := &net.ListenConfig{
    // Control permite acceder al file descriptor del socket
    // ANTES de que se llame bind() y listen().
    // Aqui puedes setear opciones de socket con setsockopt().
    Control: func(network, address string, c syscall.RawConn) error {
        return c.Control(func(fd uintptr) {
            // SO_REUSEADDR — permite reusar un puerto que esta en TIME_WAIT
            syscall.SetsockoptInt(int(fd), syscall.SOL_SOCKET, syscall.SO_REUSEADDR, 1)
            
            // SO_REUSEPORT — multiples procesos pueden escuchar en el mismo puerto
            // (Linux 3.9+). Util para load balancing sin reverse proxy.
            syscall.SetsockoptInt(int(fd), syscall.SOL_SOCKET, syscall.SO_REUSEPORT, 1)
        })
    },
    
    KeepAlive: 30 * time.Second, // TCP keepalive para conexiones aceptadas
}

listener, err := lc.Listen(context.Background(), "tcp", ":8080")
```

### 5.4 Errores comunes de Listen

```go
listener, err := net.Listen("tcp", ":8080")
if err != nil {
    // Errores tipicos:
    
    // "bind: address already in use"
    // — Otro proceso ya escucha en ese puerto
    // — Solucion: usar otro puerto, o matar el otro proceso
    // — Solucion temporal: SO_REUSEADDR (ver ListenConfig)
    
    // "bind: permission denied"
    // — Puertos < 1024 requieren root/sudo en Linux
    // — Solucion: usar puerto >= 1024, o dar CAP_NET_BIND_SERVICE
    
    // "bind: can't assign requested address"
    // — La IP especificada no existe en esta maquina
    // — Solucion: verificar la IP, o usar ":port" para todas las IPs
    
    fmt.Fprintf(os.Stderr, "error: %v\n", err)
    os.Exit(1)
}
```

---

## 6. TCP client/server basico

### 6.1 Echo server (el "Hello World" del networking)

```go
// ===== echo_server.go =====
// Un echo server lee lo que el cliente envia y lo devuelve igual.
// Es el programa mas basico para entender TCP en Go.

package main

import (
    "fmt"
    "io"
    "net"
    "os"
)

func main() {
    listener, err := net.Listen("tcp", ":9000")
    if err != nil {
        fmt.Fprintf(os.Stderr, "listen: %v\n", err)
        os.Exit(1)
    }
    defer listener.Close()
    fmt.Println("Echo server en :9000")
    
    for {
        conn, err := listener.Accept()
        if err != nil {
            fmt.Fprintf(os.Stderr, "accept: %v\n", err)
            continue
        }
        
        // IMPORTANTE: sin goroutine, solo atiende un cliente a la vez
        // (version secuencial para simplicidad)
        handleEcho(conn)
    }
}

func handleEcho(conn net.Conn) {
    defer conn.Close()
    fmt.Printf("Cliente conectado: %s\n", conn.RemoteAddr())
    
    // io.Copy es la forma mas eficiente de echo.
    // Lee de conn (Reader) y escribe de vuelta a conn (Writer).
    // Internamente usa un buffer de 32KB.
    // Retorna cuando el cliente cierra la conexion (EOF).
    written, err := io.Copy(conn, conn)
    
    if err != nil {
        fmt.Fprintf(os.Stderr, "echo error: %v\n", err)
    }
    fmt.Printf("Cliente %s desconectado (%d bytes)\n", conn.RemoteAddr(), written)
}
```

```go
// ===== echo_client.go =====
package main

import (
    "bufio"
    "fmt"
    "net"
    "os"
)

func main() {
    conn, err := net.Dial("tcp", "localhost:9000")
    if err != nil {
        fmt.Fprintf(os.Stderr, "dial: %v\n", err)
        os.Exit(1)
    }
    defer conn.Close()
    
    fmt.Println("Conectado al echo server. Escribe texto (Ctrl+D para salir):")
    
    // Leer de stdin y enviar al servidor
    scanner := bufio.NewScanner(os.Stdin)
    serverReader := bufio.NewReader(conn)
    
    for scanner.Scan() {
        line := scanner.Text()
        
        // Enviar al servidor (con newline como delimitador)
        fmt.Fprintf(conn, "%s\n", line)
        
        // Leer la respuesta
        response, err := serverReader.ReadString('\n')
        if err != nil {
            fmt.Fprintf(os.Stderr, "read error: %v\n", err)
            break
        }
        
        fmt.Printf("Echo: %s", response)
    }
}
```

```
PROBAR:
# Terminal 1:
go run echo_server.go

# Terminal 2:
go run echo_client.go
Hola          ← escribes
Echo: Hola    ← respuesta del servidor

# Tambien puedes usar netcat:
echo "Hello" | nc localhost 9000
# O telnet:
telnet localhost 9000
```

### 6.2 Echo server con goroutines (concurrente)

```go
// La version anterior solo maneja un cliente a la vez.
// Para manejar multiples clientes, lanzamos una goroutine por conexion.

package main

import (
    "fmt"
    "io"
    "net"
    "os"
    "sync/atomic"
)

var activeConns atomic.Int64

func main() {
    listener, err := net.Listen("tcp", ":9000")
    if err != nil {
        fmt.Fprintf(os.Stderr, "listen: %v\n", err)
        os.Exit(1)
    }
    defer listener.Close()
    fmt.Println("Echo server concurrente en :9000")
    
    for {
        conn, err := listener.Accept()
        if err != nil {
            fmt.Fprintf(os.Stderr, "accept: %v\n", err)
            continue
        }
        
        // Cada cliente en su propia goroutine
        go handleEcho(conn)
    }
}

func handleEcho(conn net.Conn) {
    defer conn.Close()
    
    n := activeConns.Add(1)
    addr := conn.RemoteAddr()
    fmt.Printf("[+] %s conectado (activas: %d)\n", addr, n)
    
    defer func() {
        n := activeConns.Add(-1)
        fmt.Printf("[-] %s desconectado (activas: %d)\n", addr, n)
    }()
    
    written, err := io.Copy(conn, conn)
    if err != nil {
        fmt.Fprintf(os.Stderr, "[!] %s error: %v\n", addr, err)
    }
    
    fmt.Printf("[i] %s: %d bytes transferidos\n", addr, written)
}
```

### 6.3 Chat server basico

```go
// Un chat server donde los mensajes de un cliente se envian a todos los demas.
// Demuestra coordinacion entre goroutines con channels.

package main

import (
    "bufio"
    "fmt"
    "net"
    "os"
    "strings"
    "sync"
    "time"
)

type Client struct {
    conn net.Conn
    name string
    ch   chan string // canal para enviar mensajes a este cliente
}

type ChatServer struct {
    clients map[*Client]bool
    mu      sync.RWMutex
    
    join    chan *Client
    leave   chan *Client
    msgChan chan string
}

func NewChatServer() *ChatServer {
    return &ChatServer{
        clients: make(map[*Client]bool),
        join:    make(chan *Client),
        leave:   make(chan *Client),
        msgChan: make(chan string, 256),
    }
}

func (s *ChatServer) Run() {
    for {
        select {
        case client := <-s.join:
            s.mu.Lock()
            s.clients[client] = true
            s.mu.Unlock()
            s.msgChan <- fmt.Sprintf("*** %s se unio al chat (%d usuarios)", client.name, len(s.clients))
            
        case client := <-s.leave:
            s.mu.Lock()
            delete(s.clients, client)
            close(client.ch)
            s.mu.Unlock()
            s.msgChan <- fmt.Sprintf("*** %s salio del chat (%d usuarios)", client.name, len(s.clients))
            
        case msg := <-s.msgChan:
            s.mu.RLock()
            for client := range s.clients {
                select {
                case client.ch <- msg:
                    // enviado
                default:
                    // buffer del cliente lleno, skip
                }
            }
            s.mu.RUnlock()
        }
    }
}

func (s *ChatServer) Broadcast(msg string) {
    s.msgChan <- msg
}

func (s *ChatServer) HandleConn(conn net.Conn) {
    defer conn.Close()
    
    // Pedir nombre
    fmt.Fprint(conn, "Ingresa tu nombre: ")
    scanner := bufio.NewScanner(conn)
    if !scanner.Scan() {
        return
    }
    name := strings.TrimSpace(scanner.Text())
    if name == "" {
        name = conn.RemoteAddr().String()
    }
    
    client := &Client{
        conn: conn,
        name: name,
        ch:   make(chan string, 64),
    }
    
    // Notificar al server
    s.join <- client
    
    // Goroutine para enviar mensajes al cliente
    go func() {
        for msg := range client.ch {
            fmt.Fprintln(conn, msg)
        }
    }()
    
    // Enviar bienvenida
    fmt.Fprintf(conn, "Bienvenido, %s! Escribe mensajes (Ctrl+C para salir):\n", name)
    
    // Leer mensajes del cliente
    for scanner.Scan() {
        text := strings.TrimSpace(scanner.Text())
        if text == "" {
            continue
        }
        
        timestamp := time.Now().Format("15:04:05")
        s.Broadcast(fmt.Sprintf("[%s] %s: %s", timestamp, name, text))
    }
    
    // Cliente se desconecto
    s.leave <- client
}

func main() {
    listener, err := net.Listen("tcp", ":9000")
    if err != nil {
        fmt.Fprintf(os.Stderr, "listen: %v\n", err)
        os.Exit(1)
    }
    defer listener.Close()
    fmt.Println("Chat server en :9000")
    
    server := NewChatServer()
    go server.Run()
    
    for {
        conn, err := listener.Accept()
        if err != nil {
            fmt.Fprintf(os.Stderr, "accept: %v\n", err)
            continue
        }
        go server.HandleConn(conn)
    }
}
```

```
PROBAR:
# Terminal 1 (servidor):
go run chat.go

# Terminal 2 (cliente 1):
nc localhost 9000
# Escribe tu nombre, luego mensajes

# Terminal 3 (cliente 2):
nc localhost 9000
# Los mensajes del cliente 1 aparecen aqui

# Terminal 4 (cliente 3):
telnet localhost 9000
# Tres personas chateando
```

---

## 7. Protocolos de aplicacion sobre TCP

TCP es un stream de bytes sin limites de mensajes. Necesitas un **protocolo de aplicacion** para saber donde empieza y termina cada mensaje.

### 7.1 Delimitador (newline)

```go
// PROTOCOLO: cada mensaje termina con '\n' (newline-delimited)
// Es el mas simple. Usado por SMTP, FTP, Redis, NDJSON.

// SERVIDOR: leer mensajes con bufio.Scanner
func handleConn(conn net.Conn) {
    defer conn.Close()
    scanner := bufio.NewScanner(conn)
    
    for scanner.Scan() { // Lee hasta '\n'
        msg := scanner.Text()
        response := processMessage(msg)
        fmt.Fprintf(conn, "%s\n", response)
    }
}

// CLIENTE: enviar con newline
fmt.Fprintf(conn, "HELLO %s\n", username)

// LIMITACION: el mensaje no puede contener '\n' en el contenido
// Para eso, usa longitud prefijada o escaping
```

### 7.2 Longitud prefijada (length-prefix)

```go
// PROTOCOLO: primero 4 bytes con el largo, luego los datos.
// Usado por muchos protocolos binarios, gRPC, websockets frames.

import "encoding/binary"

// Enviar un mensaje con longitud prefijada
func sendMessage(conn net.Conn, data []byte) error {
    // Escribir 4 bytes con la longitud (big-endian)
    length := uint32(len(data))
    if err := binary.Write(conn, binary.BigEndian, length); err != nil {
        return fmt.Errorf("write length: %w", err)
    }
    
    // Escribir los datos
    _, err := conn.Write(data)
    return err
}

// Recibir un mensaje con longitud prefijada
func recvMessage(conn net.Conn) ([]byte, error) {
    // Leer 4 bytes de longitud
    var length uint32
    if err := binary.Read(conn, binary.BigEndian, &length); err != nil {
        return nil, fmt.Errorf("read length: %w", err)
    }
    
    // Validar longitud (evitar DoS con longitud gigante)
    const maxSize = 10 * 1024 * 1024 // 10 MB maximo
    if length > maxSize {
        return nil, fmt.Errorf("message too large: %d bytes (max %d)", length, maxSize)
    }
    
    // Leer exactamente 'length' bytes
    data := make([]byte, length)
    _, err := io.ReadFull(conn, data)
    if err != nil {
        return nil, fmt.Errorf("read data: %w", err)
    }
    
    return data, nil
}
```

```
FORMATO EN EL WIRE:

  ┌──────────────────┬──────────────────────────────────┐
  │ Length (4 bytes)  │ Data (Length bytes)               │
  │ 00 00 00 0D      │ Hello, World!                    │
  └──────────────────┴──────────────────────────────────┘
  
  Ventajas:
  - El receptor sabe exactamente cuantos bytes leer
  - Los datos pueden contener cualquier byte (incluido '\n', '\0')
  - Eficiente: un solo allocation del tamano correcto
  
  Desventajas:
  - No es human-readable (no puedes depurar con telnet)
  - Requiere acuerdo en byte order (big-endian es la convencion de red)
```

### 7.3 Protocolo tipo-longitud-valor (TLV)

```go
// PROTOCOLO: tipo (1 byte) + longitud (4 bytes) + valor
// Permite multiples tipos de mensajes en el mismo stream.

type MessageType byte

const (
    MsgChat    MessageType = 0x01
    MsgJoin    MessageType = 0x02
    MsgLeave   MessageType = 0x03
    MsgPing    MessageType = 0x04
    MsgPong    MessageType = 0x05
    MsgFile    MessageType = 0x06
)

type Message struct {
    Type    MessageType
    Payload []byte
}

func sendMsg(conn net.Conn, msg Message) error {
    // Header: type (1) + length (4)
    header := make([]byte, 5)
    header[0] = byte(msg.Type)
    binary.BigEndian.PutUint32(header[1:5], uint32(len(msg.Payload)))
    
    if _, err := conn.Write(header); err != nil {
        return err
    }
    if len(msg.Payload) > 0 {
        _, err := conn.Write(msg.Payload)
        return err
    }
    return nil
}

func recvMsg(conn net.Conn) (Message, error) {
    // Leer header
    header := make([]byte, 5)
    if _, err := io.ReadFull(conn, header); err != nil {
        return Message{}, err
    }
    
    msgType := MessageType(header[0])
    length := binary.BigEndian.Uint32(header[1:5])
    
    if length > 10*1024*1024 {
        return Message{}, fmt.Errorf("message too large: %d", length)
    }
    
    // Leer payload
    payload := make([]byte, length)
    if length > 0 {
        if _, err := io.ReadFull(conn, payload); err != nil {
            return Message{}, err
        }
    }
    
    return Message{Type: msgType, Payload: payload}, nil
}
```

```
FORMATO TLV EN EL WIRE:

  ┌──────┬──────────────────┬──────────────────────────┐
  │ Type │ Length (4 bytes)  │ Value (Length bytes)      │
  │ 0x01 │ 00 00 00 05      │ Hello                    │
  └──────┴──────────────────┴──────────────────────────┘
  
  ┌──────┬──────────────────┬──────────────────────────┐
  │ 0x04 │ 00 00 00 00      │ (sin payload — ping)    │
  └──────┴──────────────────┴──────────────────────────┘
```

### 7.4 Protocolo de texto con comandos

```go
// PROTOCOLO: comandos de texto linea por linea (estilo Redis/SMTP/FTP)
// Formato: COMANDO arg1 arg2 ...\n
// Respuesta: +OK resultado\n o -ERR mensaje\n

type TextProtocol struct {
    conn    net.Conn
    scanner *bufio.Scanner
    writer  *bufio.Writer
}

func NewTextProtocol(conn net.Conn) *TextProtocol {
    return &TextProtocol{
        conn:    conn,
        scanner: bufio.NewScanner(conn),
        writer:  bufio.NewWriter(conn),
    }
}

func (p *TextProtocol) ReadCommand() (string, []string, error) {
    if !p.scanner.Scan() {
        if err := p.scanner.Err(); err != nil {
            return "", nil, err
        }
        return "", nil, io.EOF
    }
    
    parts := strings.Fields(p.scanner.Text())
    if len(parts) == 0 {
        return "", nil, nil
    }
    
    cmd := strings.ToUpper(parts[0])
    args := parts[1:]
    return cmd, args, nil
}

func (p *TextProtocol) WriteOK(msg string) error {
    _, err := fmt.Fprintf(p.writer, "+OK %s\r\n", msg)
    if err != nil {
        return err
    }
    return p.writer.Flush()
}

func (p *TextProtocol) WriteError(msg string) error {
    _, err := fmt.Fprintf(p.writer, "-ERR %s\r\n", msg)
    if err != nil {
        return err
    }
    return p.writer.Flush()
}

// Uso en el servidor:
func handleClient(conn net.Conn) {
    defer conn.Close()
    proto := NewTextProtocol(conn)
    proto.WriteOK("Welcome to KV Store v1.0")
    
    store := make(map[string]string)
    
    for {
        cmd, args, err := proto.ReadCommand()
        if err != nil {
            break
        }
        
        switch cmd {
        case "SET":
            if len(args) != 2 {
                proto.WriteError("SET requires key and value")
                continue
            }
            store[args[0]] = args[1]
            proto.WriteOK("stored")
            
        case "GET":
            if len(args) != 1 {
                proto.WriteError("GET requires key")
                continue
            }
            val, ok := store[args[0]]
            if !ok {
                proto.WriteError("key not found")
            } else {
                proto.WriteOK(val)
            }
            
        case "DEL":
            if len(args) != 1 {
                proto.WriteError("DEL requires key")
                continue
            }
            delete(store, args[0])
            proto.WriteOK("deleted")
            
        case "KEYS":
            keys := make([]string, 0, len(store))
            for k := range store {
                keys = append(keys, k)
            }
            proto.WriteOK(strings.Join(keys, " "))
            
        case "QUIT":
            proto.WriteOK("bye")
            return
            
        default:
            proto.WriteError(fmt.Sprintf("unknown command: %s", cmd))
        }
    }
}

// Probar con telnet:
// $ telnet localhost 9000
// +OK Welcome to KV Store v1.0
// SET name Alice
// +OK stored
// GET name
// +OK Alice
// QUIT
// +OK bye
```

### 7.5 Protocolo JSON sobre TCP

```go
// PROTOCOLO: cada mensaje es un JSON object terminado en '\n' (NDJSON sobre TCP)
// Facil de parsear, facil de debuggear, human-readable.

type Request struct {
    ID     int             `json:"id"`
    Method string          `json:"method"`
    Params json.RawMessage `json:"params,omitempty"`
}

type Response struct {
    ID     int             `json:"id"`
    Result json.RawMessage `json:"result,omitempty"`
    Error  string          `json:"error,omitempty"`
}

func handleJSONClient(conn net.Conn) {
    defer conn.Close()
    
    decoder := json.NewDecoder(conn)
    encoder := json.NewEncoder(conn)
    
    for {
        var req Request
        if err := decoder.Decode(&req); err != nil {
            if err == io.EOF {
                break
            }
            // Enviar error
            encoder.Encode(Response{Error: fmt.Sprintf("invalid json: %v", err)})
            break
        }
        
        // Procesar request
        resp := processRequest(req)
        
        if err := encoder.Encode(resp); err != nil {
            break
        }
    }
}

func processRequest(req Request) Response {
    switch req.Method {
    case "echo":
        return Response{ID: req.ID, Result: req.Params}
        
    case "time":
        now, _ := json.Marshal(time.Now().Format(time.RFC3339))
        return Response{ID: req.ID, Result: now}
        
    case "add":
        var nums [2]float64
        if err := json.Unmarshal(req.Params, &nums); err != nil {
            return Response{ID: req.ID, Error: "params must be [num1, num2]"}
        }
        result, _ := json.Marshal(nums[0] + nums[1])
        return Response{ID: req.ID, Result: result}
        
    default:
        return Response{ID: req.ID, Error: fmt.Sprintf("unknown method: %s", req.Method)}
    }
}

// Probar:
// echo '{"id":1,"method":"time"}' | nc localhost 9000
// echo '{"id":2,"method":"add","params":[3,4]}' | nc localhost 9000
```

---

## 8. UDP en Go

### 8.1 UDP client

```go
// UDP no tiene "conexion" — solo envias y recibes datagramas.
// Pero Go permite usar Dial("udp",...) para simplificar el uso
// cuando hablas con un solo servidor.

package main

import (
    "fmt"
    "net"
    "os"
    "time"
)

func main() {
    // Metodo 1: net.Dial("udp", ...) — para un solo destinatario
    // Crea un "connected" UDP socket: Read y Write van siempre al mismo addr
    conn, err := net.Dial("udp", "127.0.0.1:9000")
    if err != nil {
        fmt.Fprintf(os.Stderr, "dial: %v\n", err)
        os.Exit(1)
    }
    defer conn.Close()
    
    // Enviar datagrama
    msg := []byte("Hello UDP!")
    _, err = conn.Write(msg)
    if err != nil {
        fmt.Fprintf(os.Stderr, "write: %v\n", err)
        os.Exit(1)
    }
    
    // Esperar respuesta con timeout
    conn.SetReadDeadline(time.Now().Add(3 * time.Second))
    buf := make([]byte, 1024)
    n, err := conn.Read(buf)
    if err != nil {
        fmt.Fprintf(os.Stderr, "read: %v\n", err)
        os.Exit(1)
    }
    
    fmt.Printf("Respuesta: %s\n", buf[:n])
}
```

### 8.2 UDP server

```go
// Un servidor UDP usa net.ListenPacket (no net.Listen, que es TCP).
// ListenPacket retorna net.PacketConn en vez de net.Listener.

package main

import (
    "fmt"
    "net"
    "os"
    "strings"
)

func main() {
    // Escuchar UDP
    pc, err := net.ListenPacket("udp", ":9000")
    if err != nil {
        fmt.Fprintf(os.Stderr, "listen: %v\n", err)
        os.Exit(1)
    }
    defer pc.Close()
    fmt.Println("UDP server en :9000")
    
    buf := make([]byte, 65535) // maximo tamano de datagrama UDP
    
    for {
        // ReadFrom retorna los datos Y la direccion del remitente
        n, addr, err := pc.ReadFrom(buf)
        if err != nil {
            fmt.Fprintf(os.Stderr, "read: %v\n", err)
            continue
        }
        
        msg := string(buf[:n])
        fmt.Printf("[%s] Recibido: %s\n", addr, msg)
        
        // Responder al remitente
        response := strings.ToUpper(msg)
        _, err = pc.WriteTo([]byte(response), addr)
        if err != nil {
            fmt.Fprintf(os.Stderr, "write to %s: %v\n", addr, err)
        }
    }
}

// NOTA: No hay Accept() ni goroutines por conexion en UDP.
// Un solo socket maneja todos los clientes.
// Si el procesamiento es lento, lanza goroutines para cada datagrama:
//
// go func(data []byte, addr net.Addr) {
//     response := heavyProcess(data)
//     pc.WriteTo(response, addr)
// }(buf[:n], addr)
//
// Pero cuidado: los datagramas comparten el buffer.
// Debes copiar los datos si los procesas en goroutine.
```

### 8.3 net.PacketConn interface

```go
// net.PacketConn es la interfaz para sockets de datagramas (UDP, Unix datagram)

type PacketConn interface {
    // ReadFrom lee un datagrama y retorna el remitente
    ReadFrom(p []byte) (n int, addr Addr, err error)
    
    // WriteTo envia un datagrama a la direccion indicada
    WriteTo(p []byte, addr Addr) (n int, err error)
    
    // Close cierra el socket
    Close() error
    
    // LocalAddr retorna la direccion local
    LocalAddr() Addr
    
    // SetDeadline, SetReadDeadline, SetWriteDeadline
    // funcionan igual que en net.Conn
    SetDeadline(t time.Time) error
    SetReadDeadline(t time.Time) error
    SetWriteDeadline(t time.Time) error
}

// DIFERENCIA CLAVE:
// net.Conn (TCP):        Read(buf) / Write(buf) — una conexion, un peer
// net.PacketConn (UDP):  ReadFrom(buf) / WriteTo(buf, addr) — multiples peers
```

### 8.4 UDP vs TCP en Go — comparacion de APIs

```
┌───────────────────────┬────────────────────────────┬──────────────────────────────┐
│ Operacion             │ TCP                        │ UDP                          │
├───────────────────────┼────────────────────────────┼──────────────────────────────┤
│ Escuchar              │ net.Listen("tcp", addr)    │ net.ListenPacket("udp",addr) │
│ Retorna               │ net.Listener               │ net.PacketConn               │
│ Aceptar conexion      │ listener.Accept() → Conn   │ (no aplica)                  │
│ Leer datos            │ conn.Read(buf)             │ pc.ReadFrom(buf) → addr      │
│ Escribir datos        │ conn.Write(buf)            │ pc.WriteTo(buf, addr)        │
│ Conectar como cliente │ net.Dial("tcp", addr)      │ net.Dial("udp", addr)        │
│ Goroutines            │ 1 por conexion             │ 1 total (o por datagrama)    │
│ Cerrar                │ conn.Close()               │ pc.Close()                   │
│ Tipo especifico       │ *net.TCPConn               │ *net.UDPConn                 │
│ Metodos extras        │ SetNoDelay, SetLinger      │ ReadFromUDP, WriteToUDP      │
└───────────────────────┴────────────────────────────┴──────────────────────────────┘
```

---

## 9. DNS — resolucion de nombres

### 9.1 Lookups basicos

```go
package main

import (
    "fmt"
    "net"
)

func main() {
    // LookupHost — resolver hostname a IPs
    ips, err := net.LookupHost("google.com")
    if err != nil {
        fmt.Printf("error: %v\n", err)
        return
    }
    fmt.Println("IPs de google.com:")
    for _, ip := range ips {
        fmt.Printf("  %s\n", ip)
    }
    // Ejemplo output:
    //   142.250.80.46
    //   2607:f8b0:4004:800::200e
    
    // LookupIP — como LookupHost pero retorna net.IP (tipado)
    addrs, err := net.LookupIP("google.com")
    for _, addr := range addrs {
        fmt.Printf("  IP: %s (IPv4: %v)\n", addr, addr.To4() != nil)
    }
    
    // DNS reverso — IP a hostname
    names, err := net.LookupAddr("8.8.8.8")
    fmt.Printf("Reverse DNS de 8.8.8.8: %v\n", names)
    // ["dns.google."]
    
    // Lookup MX records (mail servers)
    mxs, err := net.LookupMX("google.com")
    for _, mx := range mxs {
        fmt.Printf("  MX: %s (priority %d)\n", mx.Host, mx.Pref)
    }
    
    // Lookup TXT records
    txts, err := net.LookupTXT("google.com")
    for _, txt := range txts {
        fmt.Printf("  TXT: %s\n", txt)
    }
    
    // Lookup NS records (nameservers)
    nss, err := net.LookupNS("google.com")
    for _, ns := range nss {
        fmt.Printf("  NS: %s\n", ns.Host)
    }
    
    // Lookup CNAME
    cname, err := net.LookupCNAME("www.google.com")
    fmt.Printf("CNAME de www.google.com: %s\n", cname)
    
    // Lookup SRV (service discovery)
    _, srvs, err := net.LookupSRV("xmpp-server", "tcp", "google.com")
    for _, srv := range srvs {
        fmt.Printf("  SRV: %s:%d (priority %d, weight %d)\n",
            srv.Target, srv.Port, srv.Priority, srv.Weight)
    }
}
```

### 9.2 Resolver custom

```go
// net.Resolver permite configurar como se resuelven nombres DNS.

resolver := &net.Resolver{
    // PreferGo usa el resolver puro de Go (no cgo/libc).
    // Ventajas: sin cgo, mas portable, no bloquea threads del SO.
    // Desventajas: no lee nsswitch.conf, puede diferir de lo que ve el SO.
    PreferGo: true,
    
    // Dial permite usar un servidor DNS custom
    Dial: func(ctx context.Context, network, address string) (net.Conn, error) {
        // Usar un DNS custom (ej: Cloudflare 1.1.1.1)
        d := &net.Dialer{Timeout: 3 * time.Second}
        return d.DialContext(ctx, "udp", "1.1.1.1:53")
    },
}

// Usar el resolver custom
ips, err := resolver.LookupHost(context.Background(), "example.com")

// Con timeout via contexto
ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
defer cancel()
ips, err = resolver.LookupHost(ctx, "example.com")
```

### 9.3 Go resolver vs cgo resolver

```
┌──────────────────────┬──────────────────────────┬──────────────────────────┐
│ Aspecto              │ Go resolver (PreferGo)   │ cgo resolver (default)   │
├──────────────────────┼──────────────────────────┼──────────────────────────┤
│ Implementacion       │ Pure Go                  │ Llama a getaddrinfo()    │
│ Lee /etc/resolv.conf │ Si                       │ Si                       │
│ Lee /etc/hosts       │ Si                       │ Si                       │
│ Lee nsswitch.conf    │ Parcial                  │ Si (completo)            │
│ Bloquea OS thread    │ No (goroutine)           │ Si (un thread por query) │
│ Cross-compile        │ Si (CGO_ENABLED=0)       │ No (necesita libc)       │
│ Forzar               │ GODEBUG=netdns=go        │ GODEBUG=netdns=cgo       │
│ mDNS (.local)        │ No                       │ Depende de nsswitch      │
│ LDAP/NIS             │ No                       │ Si (via nsswitch)        │
└──────────────────────┴──────────────────────────┴──────────────────────────┘

En la mayoria de los casos, Go elige automaticamente el resolver correcto.
Puedes forzar uno con:
  GODEBUG=netdns=go    → siempre Go resolver
  GODEBUG=netdns=cgo   → siempre cgo resolver
  GODEBUG=netdns=go+2  → Go resolver con logging verbose
```

---

## 10. Utilidades de red

### 10.1 net.IP y manipulacion de direcciones

```go
// net.IP representa una direccion IP (v4 o v6)

// Parsear IPs
ip := net.ParseIP("192.168.1.1")
if ip == nil {
    fmt.Println("IP invalida")
}

ipv6 := net.ParseIP("::1")
fmt.Println(ipv6)          // ::1
fmt.Println(ipv6.To4())    // <nil> (es IPv6 pura)
fmt.Println(ipv6.To16())   // ::1 (representacion de 16 bytes)

// Verificar tipo
ip4 := net.ParseIP("10.0.0.1")
fmt.Println(ip4.To4() != nil)              // true — es IPv4
fmt.Println(ip4.IsLoopback())              // false
fmt.Println(ip4.IsPrivate())               // true (10.x.x.x)
fmt.Println(ip4.IsGlobalUnicast())         // true
fmt.Println(net.ParseIP("127.0.0.1").IsLoopback()) // true

// Comparer IPs
ip1 := net.ParseIP("192.168.1.1")
ip2 := net.ParseIP("192.168.1.1")
fmt.Println(ip1.Equal(ip2)) // true
// NOTA: no usar == porque los slices no son comparables con ==

// Redes (CIDR)
_, network, err := net.ParseCIDR("192.168.1.0/24")
fmt.Println(network)         // 192.168.1.0/24
fmt.Println(network.IP)      // 192.168.1.0
fmt.Println(network.Mask)    // ffffff00

// Verificar si una IP esta en una red
contains := network.Contains(net.ParseIP("192.168.1.42"))
fmt.Println(contains) // true
contains = network.Contains(net.ParseIP("10.0.0.1"))
fmt.Println(contains) // false
```

### 10.2 Interfaces de red

```go
// Listar interfaces de red de la maquina

interfaces, err := net.Interfaces()
if err != nil {
    log.Fatal(err)
}

for _, iface := range interfaces {
    fmt.Printf("Interface: %s\n", iface.Name)
    fmt.Printf("  HW Addr: %s\n", iface.HardwareAddr)
    fmt.Printf("  Flags: %v\n", iface.Flags)
    fmt.Printf("  MTU: %d\n", iface.MTU)
    
    addrs, _ := iface.Addrs()
    for _, addr := range addrs {
        fmt.Printf("  Addr: %s\n", addr.String())
    }
    fmt.Println()
}

// Output ejemplo:
// Interface: lo
//   HW Addr: 
//   Flags: up|loopback
//   MTU: 65536
//   Addr: 127.0.0.1/8
//   Addr: ::1/128
//
// Interface: eth0
//   HW Addr: 02:42:ac:11:00:02
//   Flags: up|broadcast|multicast
//   MTU: 1500
//   Addr: 172.17.0.2/16

// Obtener IPs locales rapido
addrs, _ := net.InterfaceAddrs()
for _, addr := range addrs {
    if ipNet, ok := addr.(*net.IPNet); ok && !ipNet.IP.IsLoopback() {
        if ipNet.IP.To4() != nil {
            fmt.Printf("Local IPv4: %s\n", ipNet.IP)
        }
    }
}
```

### 10.3 Utilidades de host/port

```go
// JoinHostPort y SplitHostPort manejan IPv6 correctamente

// JoinHostPort — SIEMPRE usar esto en vez de fmt.Sprintf
addr := net.JoinHostPort("192.168.1.1", "8080")
// "192.168.1.1:8080"

addr = net.JoinHostPort("::1", "8080")
// "[::1]:8080"  ← brackets automaticos para IPv6

addr = net.JoinHostPort("example.com", "https")
// "example.com:https"  ← tambien acepta nombres de servicio

// SplitHostPort
host, port, err := net.SplitHostPort("192.168.1.1:8080")
// host="192.168.1.1", port="8080"

host, port, err = net.SplitHostPort("[::1]:8080")
// host="::1", port="8080"

// LookupPort — nombre de servicio a numero
port, err := net.LookupPort("tcp", "https")
// port = 443

port, err = net.LookupPort("tcp", "ssh")
// port = 22
```

---

## 11. Unix domain sockets

Los Unix domain sockets permiten comunicacion entre procesos en la misma maquina. Son mas rapidos que TCP loopback porque no pasan por la pila de red.

### 11.1 Stream sockets (como TCP pero local)

```go
// ===== unix_server.go =====
package main

import (
    "bufio"
    "fmt"
    "net"
    "os"
    "strings"
)

const socketPath = "/tmp/myapp.sock"

func main() {
    // Borrar socket anterior si existe
    os.Remove(socketPath)
    
    listener, err := net.Listen("unix", socketPath)
    if err != nil {
        fmt.Fprintf(os.Stderr, "listen: %v\n", err)
        os.Exit(1)
    }
    defer listener.Close()
    defer os.Remove(socketPath) // Cleanup al salir
    
    // Setear permisos del socket (solo owner puede conectar)
    os.Chmod(socketPath, 0600)
    
    fmt.Printf("Unix socket server en %s\n", socketPath)
    
    for {
        conn, err := listener.Accept()
        if err != nil {
            fmt.Fprintf(os.Stderr, "accept: %v\n", err)
            continue
        }
        go handleUnix(conn)
    }
}

func handleUnix(conn net.Conn) {
    defer conn.Close()
    scanner := bufio.NewScanner(conn)
    
    for scanner.Scan() {
        msg := scanner.Text()
        response := fmt.Sprintf("Procesado: %s", strings.ToUpper(msg))
        fmt.Fprintf(conn, "%s\n", response)
    }
}
```

```go
// ===== unix_client.go =====
package main

import (
    "bufio"
    "fmt"
    "net"
    "os"
)

func main() {
    conn, err := net.Dial("unix", "/tmp/myapp.sock")
    if err != nil {
        fmt.Fprintf(os.Stderr, "dial: %v\n", err)
        os.Exit(1)
    }
    defer conn.Close()
    
    // Enviar y recibir
    fmt.Fprintf(conn, "hello world\n")
    
    reader := bufio.NewReader(conn)
    response, _ := reader.ReadString('\n')
    fmt.Print(response) // "Procesado: HELLO WORLD"
}
```

### 11.2 Ejemplo practico: hablar con Docker

```go
// Docker escucha en un Unix socket: /var/run/docker.sock
// Puedes comunicarte con el daemon sin usar la libreria de Docker.

package main

import (
    "encoding/json"
    "fmt"
    "io"
    "net"
    "net/http"
)

func main() {
    // Crear un HTTP client que se conecta via Unix socket
    client := &http.Client{
        Transport: &http.Transport{
            DialContext: func(ctx context.Context, network, addr string) (net.Conn, error) {
                return net.Dial("unix", "/var/run/docker.sock")
            },
        },
    }
    
    // Listar containers
    // El host es irrelevante (va por Unix socket), pero HTTP lo requiere
    resp, err := client.Get("http://localhost/v1.41/containers/json")
    if err != nil {
        fmt.Printf("error: %v\n", err)
        return
    }
    defer resp.Body.Close()
    
    body, _ := io.ReadAll(resp.Body)
    
    var containers []map[string]interface{}
    json.Unmarshal(body, &containers)
    
    for _, c := range containers {
        fmt.Printf("Container: %s (%.12s)\n", c["Names"], c["Id"])
    }
}
```

### 11.3 Cuando usar Unix sockets vs TCP loopback

```
┌──────────────────────┬──────────────────────────┬──────────────────────────┐
│ Aspecto              │ Unix socket              │ TCP 127.0.0.1            │
├──────────────────────┼──────────────────────────┼──────────────────────────┤
│ Rendimiento          │ ~30% mas rapido          │ Mas lento (pila de red)  │
│ Latencia             │ Menor                    │ Mayor                    │
│ Seguridad            │ Permisos de archivo      │ Cualquiera puede conectar│
│ Descubrimiento       │ Ruta fija en filesystem  │ host:port                │
│ Cross-machine        │ No (solo local)          │ Si                       │
│ Debug con curl       │ curl --unix-socket ...   │ curl http://localhost:.. │
│ Usado por            │ Docker, PostgreSQL, nginx│ La mayoria de servicios  │
│ Credentials passing  │ Si (SO_PEERCRED)         │ No                       │
│ File descriptor pass │ Si (SCM_RIGHTS)          │ No                       │
└──────────────────────┴──────────────────────────┴──────────────────────────┘
```

---

## 12. Patrones comunes

### 12.1 Patron: cliente con reconexion

```go
// Reconectarse automaticamente si la conexion se pierde

type ReconnectingClient struct {
    addr       string
    conn       net.Conn
    mu         sync.Mutex
    maxRetries int
    baseDelay  time.Duration
}

func NewReconnectingClient(addr string) *ReconnectingClient {
    return &ReconnectingClient{
        addr:       addr,
        maxRetries: 10,
        baseDelay:  time.Second,
    }
}

func (c *ReconnectingClient) Connect() error {
    c.mu.Lock()
    defer c.mu.Unlock()
    
    for attempt := 0; attempt < c.maxRetries; attempt++ {
        conn, err := net.DialTimeout("tcp", c.addr, 5*time.Second)
        if err == nil {
            c.conn = conn
            return nil
        }
        
        // Exponential backoff con jitter
        delay := c.baseDelay * time.Duration(1<<uint(attempt))
        if delay > 30*time.Second {
            delay = 30 * time.Second
        }
        // Agregar jitter: ±25%
        jitter := time.Duration(rand.Int63n(int64(delay) / 2))
        delay = delay/2 + jitter
        
        fmt.Fprintf(os.Stderr, "conexion fallida (intento %d/%d), reintentando en %v: %v\n",
            attempt+1, c.maxRetries, delay, err)
        time.Sleep(delay)
    }
    
    return fmt.Errorf("no se pudo conectar a %s despues de %d intentos", c.addr, c.maxRetries)
}

func (c *ReconnectingClient) Send(data []byte) error {
    c.mu.Lock()
    defer c.mu.Unlock()
    
    if c.conn == nil {
        return fmt.Errorf("not connected")
    }
    
    c.conn.SetWriteDeadline(time.Now().Add(5 * time.Second))
    _, err := c.conn.Write(data)
    if err != nil {
        c.conn.Close()
        c.conn = nil
        // Intentar reconectar
        go c.Connect()
        return fmt.Errorf("write failed, reconnecting: %w", err)
    }
    
    return nil
}
```

### 12.2 Patron: rate limiter por IP

```go
// Limitar la cantidad de conexiones por IP

type RateLimiter struct {
    mu       sync.Mutex
    counts   map[string]int
    maxConns int
}

func NewRateLimiter(maxConnsPerIP int) *RateLimiter {
    return &RateLimiter{
        counts:   make(map[string]int),
        maxConns: maxConnsPerIP,
    }
}

func (rl *RateLimiter) Allow(addr net.Addr) bool {
    host, _, _ := net.SplitHostPort(addr.String())
    
    rl.mu.Lock()
    defer rl.mu.Unlock()
    
    if rl.counts[host] >= rl.maxConns {
        return false
    }
    rl.counts[host]++
    return true
}

func (rl *RateLimiter) Release(addr net.Addr) {
    host, _, _ := net.SplitHostPort(addr.String())
    
    rl.mu.Lock()
    defer rl.mu.Unlock()
    
    rl.counts[host]--
    if rl.counts[host] <= 0 {
        delete(rl.counts, host)
    }
}

// Uso en el servidor:
func main() {
    listener, _ := net.Listen("tcp", ":9000")
    limiter := NewRateLimiter(5) // max 5 conexiones por IP
    
    for {
        conn, _ := listener.Accept()
        
        if !limiter.Allow(conn.RemoteAddr()) {
            fmt.Fprintf(conn, "Too many connections from your IP\n")
            conn.Close()
            continue
        }
        
        go func() {
            defer limiter.Release(conn.RemoteAddr())
            defer conn.Close()
            handleConnection(conn)
        }()
    }
}
```

### 12.3 Patron: port scanner

```go
// Escanear puertos abiertos en un host (herramienta de diagnostico)

func scanPort(host string, port int, timeout time.Duration) bool {
    addr := net.JoinHostPort(host, strconv.Itoa(port))
    conn, err := net.DialTimeout("tcp", addr, timeout)
    if err != nil {
        return false
    }
    conn.Close()
    return true
}

func scanPorts(host string, startPort, endPort int) []int {
    var open []int
    var mu sync.Mutex
    var wg sync.WaitGroup
    
    // Semaforo para limitar conexiones simultaneas
    sem := make(chan struct{}, 100)
    
    for port := startPort; port <= endPort; port++ {
        wg.Add(1)
        sem <- struct{}{} // adquirir slot
        
        go func(p int) {
            defer wg.Done()
            defer func() { <-sem }() // liberar slot
            
            if scanPort(host, p, 2*time.Second) {
                mu.Lock()
                open = append(open, p)
                mu.Unlock()
            }
        }(port)
    }
    
    wg.Wait()
    sort.Ints(open)
    return open
}

func main() {
    host := "localhost"
    if len(os.Args) > 1 {
        host = os.Args[1]
    }
    
    fmt.Printf("Escaneando %s puertos 1-1024...\n", host)
    open := scanPorts(host, 1, 1024)
    
    for _, port := range open {
        service, _ := net.LookupPort("tcp", "")
        fmt.Printf("  %d/tcp open (%s)\n", port, getServiceName(port))
    }
    fmt.Printf("\n%d puertos abiertos\n", len(open))
}

func getServiceName(port int) string {
    // Algunos servicios conocidos
    services := map[int]string{
        22: "ssh", 25: "smtp", 53: "dns", 80: "http",
        443: "https", 3306: "mysql", 5432: "postgresql",
        6379: "redis", 8080: "http-alt", 27017: "mongodb",
    }
    if name, ok := services[port]; ok {
        return name
    }
    return "unknown"
}
```

---

## 13. Comparacion con C y Rust

```
┌──────────────────────────────┬──────────────────────────┬──────────────────────────┬──────────────────────────┐
│ Aspecto                      │ C                        │ Go                       │ Rust                     │
├──────────────────────────────┼──────────────────────────┼──────────────────────────┼──────────────────────────┤
│ Crear socket                 │ socket(AF_INET,          │ (implicito en           │ TcpListener::bind()      │
│                              │   SOCK_STREAM, 0)        │  Listen/Dial)            │ TcpStream::connect()     │
│ Bind                         │ bind(fd, &addr, len)     │ (implicito en Listen)   │ (implicito en bind)      │
│ Listen                       │ listen(fd, backlog)      │ net.Listen("tcp", addr) │ TcpListener::bind(addr)  │
│ Accept                       │ accept(fd, &addr, &len)  │ listener.Accept()       │ listener.accept()        │
│ Connect                      │ connect(fd, &addr, len)  │ net.Dial("tcp", addr)   │ TcpStream::connect(addr) │
│ Read                         │ read(fd, buf, len)       │ conn.Read(buf)          │ stream.read(&mut buf)    │
│ Write                        │ write(fd, buf, len)      │ conn.Write(buf)         │ stream.write(&buf)       │
│ Close                        │ close(fd)                │ conn.Close()            │ drop(stream) / .shutdown │
│ Timeout                      │ setsockopt SO_RCVTIMEO   │ conn.SetDeadline(t)     │ set_read_timeout()       │
│ Non-blocking                 │ fcntl O_NONBLOCK         │ (automatico por runtime)│ mio/tokio (async)        │
│ Multiplexing                 │ epoll/kqueue/select      │ runtime (goroutines)    │ tokio/mio (async)        │
│ DNS lookup                   │ getaddrinfo()            │ net.LookupHost()        │ ToSocketAddrs (sync)     │
│ IPv6                         │ AF_INET6, sockaddr_in6   │ automatico ("tcp")      │ automatico               │
│ Error handling               │ errno, perror()          │ error interface          │ Result<T, io::Error>     │
│ UDP                          │ SOCK_DGRAM               │ net.ListenPacket("udp") │ UdpSocket::bind()        │
│ Unix socket                  │ AF_UNIX                  │ net.Dial("unix", path)  │ UnixStream::connect()    │
│ Socket options               │ setsockopt()             │ net.ListenConfig.Control│ socket2 crate            │
│ Modelo de concurrencia       │ thread/process per conn  │ goroutine per conn      │ async task per conn      │
│                              │ o event loop (epoll)     │ (runtime usa epoll)     │ (tokio usa epoll)        │
│ Bytes transferidos           │ sendfile() / splice()    │ io.Copy (usa sendfile)  │ io::copy (no sendfile)   │
│ Interfaz de conexion         │ int fd                   │ net.Conn interface      │ TcpStream struct         │
│ Read + Write interface       │ read()/write()           │ io.Reader/io.Writer     │ Read/Write traits        │
│ Composicion con I/O          │ fdopen() → FILE*         │ nativo (Conn=Reader)    │ nativo (impl Read)       │
│ Buffer size                  │ setsockopt SO_RCVBUF     │ net.(*TCPConn) methods  │ set_recv_buffer_size()   │
│ Keepalive                    │ setsockopt SO_KEEPALIVE   │ net.Dialer.KeepAlive    │ set_keepalive()          │
│ Complejidad setup            │ ~20 lineas minimo        │ ~5 lineas               │ ~5 lineas (sync)         │
│                              │                          │                          │ ~10 lineas (async/tokio) │
│ TLS                          │ OpenSSL (externo)        │ crypto/tls (stdlib)     │ rustls/native-tls (ext.) │
│ HTTP sobre net               │ manual o libcurl         │ net/http (stdlib)       │ hyper/reqwest (externo)  │
└──────────────────────────────┴──────────────────────────┴──────────────────────────┴──────────────────────────┘
```

### Insight: modelo de I/O

```
C (socket bloqueante):
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  connect(fd, &addr, sizeof(addr));
  write(fd, "GET / HTTP/1.0\r\n\r\n", 18);
  read(fd, buf, sizeof(buf));
  close(fd);
  // Bloqueante: el thread se duerme esperando I/O.
  // Para multiples conexiones: un thread por conexion (caro)
  // o epoll/kqueue event loop (complejo).

C (epoll, event-driven):
  // ~100 lineas de boilerplate con epoll_create, epoll_ctl,
  // epoll_wait, buffers parciales, maquina de estados...
  // Muy eficiente pero muy complejo.

Go:
  conn, _ := net.Dial("tcp", "example.com:80")
  fmt.Fprintf(conn, "GET / HTTP/1.0\r\n\r\n")
  io.Copy(os.Stdout, conn)
  conn.Close()
  // PARECE bloqueante pero el runtime usa epoll internamente.
  // Miles de goroutines bloqueadas en Read/Write usan pocos threads.
  // Lo mejor de ambos mundos: codigo simple + rendimiento asincrono.

Rust (tokio):
  let mut stream = TcpStream::connect("example.com:80").await?;
  stream.write_all(b"GET / HTTP/1.0\r\n\r\n").await?;
  let mut buf = Vec::new();
  stream.read_to_end(&mut buf).await?;
  // Asincrono explicito con async/await.
  // Mas control pero requiere runtime (tokio) y funcion coloring.
```

---

## 14. Programa completo: TCP File Transfer

Un sistema de transferencia de archivos por TCP con protocolo length-prefixed, progreso, y soporte para multiples archivos.

```go
// ===== main.go =====
package main

import (
    "encoding/binary"
    "encoding/json"
    "fmt"
    "io"
    "net"
    "os"
    "path/filepath"
    "strings"
    "sync/atomic"
    "time"
)

// --- Protocolo ---

// Cada transferencia:
// 1. Cliente envia FileHeader como JSON (length-prefixed)
// 2. Cliente envia el contenido del archivo (length-prefixed, en chunks)
// 3. Servidor responde con status

type FileHeader struct {
    Name string `json:"name"`
    Size int64  `json:"size"`
}

type TransferStatus struct {
    OK      bool   `json:"ok"`
    Message string `json:"message"`
}

const (
    maxHeaderSize = 4096
    chunkSize     = 64 * 1024 // 64 KB
)

// Enviar un bloque con length-prefix (4 bytes big-endian + datos)
func sendBlock(conn net.Conn, data []byte) error {
    length := uint32(len(data))
    if err := binary.Write(conn, binary.BigEndian, length); err != nil {
        return fmt.Errorf("write length: %w", err)
    }
    _, err := conn.Write(data)
    return err
}

// Recibir un bloque con length-prefix
func recvBlock(conn net.Conn, maxSize uint32) ([]byte, error) {
    var length uint32
    if err := binary.Read(conn, binary.BigEndian, &length); err != nil {
        return nil, fmt.Errorf("read length: %w", err)
    }
    if length > maxSize {
        return nil, fmt.Errorf("block too large: %d (max %d)", length, maxSize)
    }
    data := make([]byte, length)
    _, err := io.ReadFull(conn, data)
    return data, err
}

// --- Servidor ---

func runServer(addr string, outputDir string) error {
    listener, err := net.Listen("tcp", addr)
    if err != nil {
        return fmt.Errorf("listen: %w", err)
    }
    defer listener.Close()
    
    // Crear directorio de salida
    if err := os.MkdirAll(outputDir, 0755); err != nil {
        return fmt.Errorf("mkdir: %w", err)
    }
    
    fmt.Printf("File server escuchando en %s\n", listener.Addr())
    fmt.Printf("Archivos se guardan en: %s\n", outputDir)
    
    var totalFiles atomic.Int64
    var totalBytes atomic.Int64
    
    for {
        conn, err := listener.Accept()
        if err != nil {
            fmt.Fprintf(os.Stderr, "accept: %v\n", err)
            continue
        }
        
        go func() {
            defer conn.Close()
            addr := conn.RemoteAddr()
            
            files, bytes, err := handleReceive(conn, outputDir)
            if err != nil {
                fmt.Fprintf(os.Stderr, "[%s] Error: %v\n", addr, err)
                return
            }
            
            totalFiles.Add(int64(files))
            totalBytes.Add(bytes)
            
            fmt.Printf("[%s] Recibidos %d archivos (%s). Total: %d archivos, %s\n",
                addr, files, formatBytes(bytes),
                totalFiles.Load(), formatBytes(totalBytes.Load()))
        }()
    }
}

func handleReceive(conn net.Conn, outputDir string) (int, int64, error) {
    filesReceived := 0
    var bytesReceived int64
    
    for {
        // Leer header
        conn.SetReadDeadline(time.Now().Add(30 * time.Second))
        headerData, err := recvBlock(conn, maxHeaderSize)
        if err != nil {
            if err == io.EOF || strings.Contains(err.Error(), "EOF") {
                break // Cliente termino de enviar
            }
            return filesReceived, bytesReceived, fmt.Errorf("recv header: %w", err)
        }
        
        var header FileHeader
        if err := json.Unmarshal(headerData, &header); err != nil {
            sendStatus(conn, false, "invalid header")
            return filesReceived, bytesReceived, fmt.Errorf("unmarshal header: %w", err)
        }
        
        // Sanitizar nombre de archivo (seguridad)
        safeName := filepath.Base(header.Name) // Eliminar path traversal
        if safeName == "." || safeName == ".." {
            sendStatus(conn, false, "invalid filename")
            continue
        }
        
        destPath := filepath.Join(outputDir, safeName)
        
        fmt.Printf("  Recibiendo: %s (%s)\n", safeName, formatBytes(header.Size))
        
        // Recibir contenido del archivo
        conn.SetReadDeadline(time.Now().Add(5 * time.Minute))
        
        f, err := os.Create(destPath)
        if err != nil {
            sendStatus(conn, false, fmt.Sprintf("create file: %v", err))
            return filesReceived, bytesReceived, err
        }
        
        var received int64
        for received < header.Size {
            remaining := header.Size - received
            readSize := int64(chunkSize)
            if remaining < readSize {
                readSize = remaining
            }
            
            chunk, err := recvBlock(conn, uint32(readSize)+4) // +4 por overhead
            if err != nil {
                f.Close()
                os.Remove(destPath)
                return filesReceived, bytesReceived, fmt.Errorf("recv chunk: %w", err)
            }
            
            if _, err := f.Write(chunk); err != nil {
                f.Close()
                os.Remove(destPath)
                return filesReceived, bytesReceived, fmt.Errorf("write file: %w", err)
            }
            
            received += int64(len(chunk))
        }
        
        f.Close()
        bytesReceived += received
        filesReceived++
        
        sendStatus(conn, true, fmt.Sprintf("saved %s (%s)", safeName, formatBytes(received)))
    }
    
    return filesReceived, bytesReceived, nil
}

func sendStatus(conn net.Conn, ok bool, msg string) {
    status := TransferStatus{OK: ok, Message: msg}
    data, _ := json.Marshal(status)
    sendBlock(conn, data)
}

// --- Cliente ---

func runClient(addr string, files []string) error {
    conn, err := net.DialTimeout("tcp", addr, 10*time.Second)
    if err != nil {
        return fmt.Errorf("dial: %w", err)
    }
    defer conn.Close()
    
    fmt.Printf("Conectado a %s\n", addr)
    
    var totalBytes int64
    
    for _, filePath := range files {
        info, err := os.Stat(filePath)
        if err != nil {
            fmt.Fprintf(os.Stderr, "skip %s: %v\n", filePath, err)
            continue
        }
        if info.IsDir() {
            fmt.Fprintf(os.Stderr, "skip %s: es un directorio\n", filePath)
            continue
        }
        
        bytes, err := sendFile(conn, filePath, info)
        if err != nil {
            return fmt.Errorf("send %s: %w", filePath, err)
        }
        
        totalBytes += bytes
    }
    
    fmt.Printf("\nTransferencia completada: %d archivos, %s\n", len(files), formatBytes(totalBytes))
    return nil
}

func sendFile(conn net.Conn, filePath string, info os.FileInfo) (int64, error) {
    // Enviar header
    header := FileHeader{
        Name: filepath.Base(filePath),
        Size: info.Size(),
    }
    headerData, _ := json.Marshal(header)
    
    if err := sendBlock(conn, headerData); err != nil {
        return 0, fmt.Errorf("send header: %w", err)
    }
    
    // Enviar contenido en chunks
    f, err := os.Open(filePath)
    if err != nil {
        return 0, fmt.Errorf("open: %w", err)
    }
    defer f.Close()
    
    buf := make([]byte, chunkSize)
    var sent int64
    start := time.Now()
    
    for sent < info.Size() {
        n, err := f.Read(buf)
        if n > 0 {
            if err := sendBlock(conn, buf[:n]); err != nil {
                return sent, fmt.Errorf("send chunk: %w", err)
            }
            sent += int64(n)
            
            // Mostrar progreso
            pct := float64(sent) / float64(info.Size()) * 100
            elapsed := time.Since(start)
            speed := float64(sent) / elapsed.Seconds()
            fmt.Fprintf(os.Stderr, "\r  %s: %.1f%% (%s/%s) %s/s",
                header.Name, pct,
                formatBytes(sent), formatBytes(info.Size()),
                formatBytes(int64(speed)))
        }
        if err == io.EOF {
            break
        }
        if err != nil {
            return sent, fmt.Errorf("read file: %w", err)
        }
    }
    fmt.Fprintln(os.Stderr)
    
    // Esperar status del servidor
    conn.SetReadDeadline(time.Now().Add(30 * time.Second))
    statusData, err := recvBlock(conn, maxHeaderSize)
    if err != nil {
        return sent, fmt.Errorf("recv status: %w", err)
    }
    
    var status TransferStatus
    json.Unmarshal(statusData, &status)
    
    if !status.OK {
        return sent, fmt.Errorf("server error: %s", status.Message)
    }
    
    fmt.Printf("  ✓ %s\n", status.Message)
    return sent, nil
}

// --- Utilidades ---

func formatBytes(b int64) string {
    const (
        KB = 1024
        MB = KB * 1024
        GB = MB * 1024
    )
    switch {
    case b >= GB:
        return fmt.Sprintf("%.2f GB", float64(b)/float64(GB))
    case b >= MB:
        return fmt.Sprintf("%.2f MB", float64(b)/float64(MB))
    case b >= KB:
        return fmt.Sprintf("%.2f KB", float64(b)/float64(KB))
    default:
        return fmt.Sprintf("%d B", b)
    }
}

func printUsage() {
    fmt.Fprintf(os.Stderr, `uso: filetransfer <modo> [opciones]

Modos:
  serve <addr> [output-dir]    Iniciar servidor de recepcion
  send <addr> <archivo...>     Enviar archivos al servidor

Ejemplos:
  filetransfer serve :9000 ./received
  filetransfer send localhost:9000 file1.txt file2.pdf
  filetransfer send 192.168.1.10:9000 *.log
`)
}

func main() {
    if len(os.Args) < 2 {
        printUsage()
        os.Exit(1)
    }
    
    var err error
    
    switch os.Args[1] {
    case "serve":
        addr := ":9000"
        outputDir := "./received"
        if len(os.Args) > 2 {
            addr = os.Args[2]
        }
        if len(os.Args) > 3 {
            outputDir = os.Args[3]
        }
        err = runServer(addr, outputDir)
        
    case "send":
        if len(os.Args) < 4 {
            fmt.Fprintln(os.Stderr, "uso: filetransfer send <addr> <archivo...>")
            os.Exit(1)
        }
        err = runClient(os.Args[2], os.Args[3:])
        
    default:
        printUsage()
        os.Exit(1)
    }
    
    if err != nil {
        fmt.Fprintf(os.Stderr, "error: %v\n", err)
        os.Exit(1)
    }
}
```

```
PROBAR EL PROGRAMA:

# Terminal 1 (servidor):
go run main.go serve :9000 ./received

# Terminal 2 (cliente):
# Crear archivos de prueba
echo "Hola mundo" > test1.txt
dd if=/dev/urandom of=test2.bin bs=1M count=10
echo '{"name":"Alice"}' > test3.json

# Enviar archivos
go run main.go send localhost:9000 test1.txt test2.bin test3.json

# Output del cliente:
#   Conectado a localhost:9000
#     test1.txt: 100.0% (11 B/11 B) 11 B/s
#     ✓ saved test1.txt (11 B)
#     test2.bin: 100.0% (10.00 MB/10.00 MB) 450.23 MB/s
#     ✓ saved test2.bin (10.00 MB)
#     test3.json: 100.0% (18 B/18 B) 18 B/s
#     ✓ saved test3.json (18 B)
#   Transferencia completada: 3 archivos, 10.00 MB

# Verificar archivos recibidos
ls -la received/
diff test1.txt received/test1.txt  # sin diferencias
md5sum test2.bin received/test2.bin  # hashes iguales

CONCEPTOS DEMOSTRADOS:
  net.Listen / listener.Accept:  servidor TCP
  net.DialTimeout:               cliente con timeout
  conn.Read / conn.Write:        transferencia de datos
  conn.SetReadDeadline:          timeouts por operacion
  encoding/binary:               length-prefix protocol (big-endian)
  io.ReadFull:                   leer exactamente N bytes
  encoding/json:                 headers y status como JSON
  goroutines:                    un goroutine por conexion
  atomic.Int64:                  contadores thread-safe
  filepath.Base:                 sanitizar nombres (path traversal)
  os.Stderr:                     progreso separado de datos
  Chunked transfer:              archivos grandes en bloques de 64KB
  Protocolo custom:              header → chunks → status
```

---

## 15. Ejercicios

### Ejercicio 1: Key-Value Store TCP
Crea un servidor TCP que implemente un key-value store con protocolo de texto:
- Comandos: `SET key value`, `GET key`, `DEL key`, `KEYS [prefix]`, `TTL key seconds` (expiracion), `PING`, `QUIT`
- Respuestas: `+OK valor` para exito, `-ERR mensaje` para error, `*N` seguido de N lineas para listas
- Soporte multiples clientes simultaneos (goroutine por conexion)
- Mutex para proteger el mapa compartido
- Implementa expiracion de claves (un goroutine de background que limpia expiradas cada segundo)
- Testea con `telnet localhost 9000` o `nc localhost 9000`

### Ejercicio 2: Port Scanner avanzado
Crea un port scanner CLI que:
- Acepte un host y rango de puertos como argumentos: `./scanner -host example.com -ports 1-1024`
- Use goroutines con semaforo para limitar conexiones simultaneas (`-workers 200`)
- Muestre los servicios conocidos para cada puerto abierto (ssh, http, https, etc.)
- Reporte el tiempo total de escaneo
- Soporte flag `-timeout` para el timeout por conexion
- Intente leer el banner del servicio (primeros bytes que envia el servidor al conectar) con `-banner`
- Output en tabla o JSON (`-format table|json`)

### Ejercicio 3: UDP Chat con broadcast
Crea un chat por UDP donde:
- El servidor escucha en un puerto UDP y mantiene un registro de clientes activos
- Cada mensaje recibido se reenvía a todos los demas clientes (broadcast)
- Los clientes envian un datagrama "JOIN nombre" al conectar y "LEAVE" al salir
- El servidor detecta clientes inactivos (sin mensaje en 60s) y los elimina
- Cada datagrama tiene formato: `nombre: mensaje`
- Implementa un heartbeat: clientes envian "PING" cada 15s, servidor responde "PONG"
- Maneja el caso de datagramas perdidos (UDP no garantiza entrega)

### Ejercicio 4: TCP Proxy con logging
Crea un proxy TCP transparente que:
- Escuche en un puerto local y reenvie conexiones a un destino remoto
- Uso: `./proxy -listen :8080 -target example.com:80`
- Logee cada conexion: IP origen, bytes transferidos, duracion
- Use `io.Copy` en ambas direcciones (cliente→servidor y servidor→cliente) con goroutines
- Soporte un flag `-hex` que muestre los primeros N bytes de cada direccion en hex dump
- Soporte un flag `-max-conns` que limite las conexiones simultaneas
- Graceful shutdown con Ctrl+C (dejar de aceptar, esperar conexiones existentes)

---

> **Siguiente**: C10/S01 - TCP/UDP, T02 - Servidores concurrentes — goroutine por conexion, patrones de timeout, graceful shutdown
