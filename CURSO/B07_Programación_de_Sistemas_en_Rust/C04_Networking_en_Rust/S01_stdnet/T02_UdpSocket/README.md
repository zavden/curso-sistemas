# `UdpSocket` — send_to, recv_from, broadcast

## Índice

1. [UDP vs TCP: cuándo usar cada uno](#1-udp-vs-tcp-cuándo-usar-cada-uno)
2. [UdpSocket — Uso básico](#2-udpsocket--uso-básico)
3. [send_to y recv_from](#3-send_to-y-recv_from)
4. [connect y send/recv](#4-connect-y-sendrecv)
5. [Broadcast](#5-broadcast)
6. [Multicast](#6-multicast)
7. [Patrones prácticos](#7-patrones-prácticos)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. UDP vs TCP: cuándo usar cada uno

```
┌─────────────────────┬───────────────────┬───────────────────┐
│ Aspecto             │ TCP               │ UDP               │
├─────────────────────┼───────────────────┼───────────────────┤
│ Conexión            │ Orientado a       │ Sin conexión      │
│                     │ conexión (3-way   │ (datagramas)      │
│                     │ handshake)        │                   │
├─────────────────────┼───────────────────┼───────────────────┤
│ Entrega             │ Garantizada       │ No garantizada    │
│                     │ (retransmisiones) │ (best-effort)     │
├─────────────────────┼───────────────────┼───────────────────┤
│ Orden               │ Garantizado       │ No garantizado    │
│                     │ (números de seq)  │ (pueden llegar    │
│                     │                   │  desordenados)    │
├─────────────────────┼───────────────────┼───────────────────┤
│ Duplicados          │ Eliminados        │ Posibles          │
├─────────────────────┼───────────────────┼───────────────────┤
│ Boundaries          │ Stream (sin       │ Datagramas (cada  │
│                     │ límites de msg)   │ send = 1 recv)    │
├─────────────────────┼───────────────────┼───────────────────┤
│ Overhead            │ Alto (headers,    │ Bajo (8 bytes     │
│                     │ state, retrans.)  │ de header)        │
├─────────────────────┼───────────────────┼───────────────────┤
│ Latencia            │ Mayor (handshake, │ Menor (envío      │
│                     │ ACKs, Nagle)      │ inmediato)        │
├─────────────────────┼───────────────────┼───────────────────┤
│ Broadcast/Multicast │ No               │ Sí               │
└─────────────────────┴───────────────────┴───────────────────┘
```

Cuándo usar UDP:
- **DNS**: consultas cortas, una pregunta = una respuesta
- **Gaming**: posición del jugador — si se pierde un frame, el siguiente lo reemplaza
- **Streaming**: audio/video — pérdida de un paquete es tolerable, latencia no
- **Métricas/logs**: StatsD, syslog — perder una métrica es aceptable
- **Discovery**: encontrar servicios en la red local (broadcast/multicast)
- **VoIP**: latencia importa más que fiabilidad

Cuándo NO usar UDP:
- Transferencia de archivos (necesitas completitud)
- APIs REST/HTTP (necesitas request/response confiable)
- Cualquier cosa donde perder un mensaje causa corrupción

```
TCP:
  SYN ──────►        Datagrama UDP:
  ◄────── SYN+ACK    ┌──────────────┐
  ACK ──────►         │ src port     │ 2 bytes
  (conexión)          │ dst port     │ 2 bytes
  DATA ────►          │ length       │ 2 bytes
  ◄──── ACK           │ checksum     │ 2 bytes
  DATA ────►          │ payload ...  │ variable
  ◄──── ACK           └──────────────┘
  FIN ─────►          Total header: 8 bytes
  ◄──── ACK           No handshake, no ACK, no retransmission
  (cierre)
```

---

## 2. `UdpSocket` — Uso básico

### 2.1 Crear y bind

```rust
use std::net::UdpSocket;

fn main() -> std::io::Result<()> {
    // Bind a un puerto específico
    let socket = UdpSocket::bind("127.0.0.1:8080")?;
    println!("Bound to {}", socket.local_addr()?);

    // Bind a puerto automático
    let socket = UdpSocket::bind("127.0.0.1:0")?;
    println!("Auto port: {}", socket.local_addr()?);

    // Bind a todas las interfaces
    let socket = UdpSocket::bind("0.0.0.0:8080")?;
    println!("All interfaces: {}", socket.local_addr()?);

    Ok(())
}
```

A diferencia de TCP, no hay "listener" ni "accept". Un solo `UdpSocket` puede
enviar y recibir datagramas de cualquier dirección.

```
TCP:                             UDP:
  Listener ───► accept()           Socket ◄───► cualquier dirección
    │             │
    └── Stream ◄──┘                Un socket para todo
    └── Stream
    └── Stream

  Un listener + N streams          Un socket para N peers
```

### 2.2 Un socket, dos roles

En UDP, el mismo socket puede actuar como "servidor" y "cliente" simultáneamente:

```rust
use std::net::UdpSocket;

fn main() -> std::io::Result<()> {
    let socket = UdpSocket::bind("127.0.0.1:9000")?;

    // Enviar un datagrama (como "cliente")
    socket.send_to(b"hello", "127.0.0.1:9001")?;

    // Recibir un datagrama (como "servidor")
    let mut buf = [0u8; 1024];
    let (n, src) = socket.recv_from(&mut buf)?;
    println!("From {}: {:?}", src, &buf[..n]);

    Ok(())
}
```

---

## 3. `send_to` y `recv_from`

### 3.1 API sin conexión

`send_to` y `recv_from` son las operaciones fundamentales de UDP. Cada llamada
envía/recibe un datagrama completo con dirección explícita:

```rust
use std::net::UdpSocket;

fn main() -> std::io::Result<()> {
    let socket = UdpSocket::bind("127.0.0.1:0")?;

    // send_to: enviar datagrama a una dirección específica
    let bytes_sent = socket.send_to(
        b"Hello, UDP!",
        "127.0.0.1:9000",
    )?;
    println!("Sent {} bytes", bytes_sent);

    // recv_from: recibir datagrama y saber de dónde viene
    let mut buf = [0u8; 65535]; // Max datagrama UDP
    let (bytes_received, source_addr) = socket.recv_from(&mut buf)?;
    println!("Received {} bytes from {}", bytes_received, source_addr);
    println!("Data: {:?}", &buf[..bytes_received]);

    Ok(())
}
```

### 3.2 Datagramas vs stream

Cada `send_to` produce exactamente un datagrama, y cada `recv_from` recibe exactamente
un datagrama:

```
TCP (stream):                    UDP (datagrama):
  write("Hello")                   send_to("Hello")
  write(" World")                  send_to(" World")

  read() puede dar:                recv_from() da:
    "Hello World" (junto)            "Hello"     (1er datagrama)
    "Hello" + " World" (separado)    " World"    (2do datagrama)
    "Hell" + "o Wor" + "ld"         NUNCA mezcla datagramas

→ TCP no preserva boundaries     → UDP preserva boundaries
→ Necesitas framing propio       → Cada send = un mensaje completo
```

```rust
use std::net::UdpSocket;
use std::thread;

fn main() -> std::io::Result<()> {
    let receiver = UdpSocket::bind("127.0.0.1:9000")?;
    let sender = UdpSocket::bind("127.0.0.1:0")?;

    // Enviar 3 datagramas separados
    sender.send_to(b"AAA", "127.0.0.1:9000")?;
    sender.send_to(b"BBBBB", "127.0.0.1:9000")?;
    sender.send_to(b"CC", "127.0.0.1:9000")?;

    thread::sleep(std::time::Duration::from_millis(100));

    // Cada recv_from recibe exactamente un datagrama
    let mut buf = [0u8; 1024];

    let (n, _) = receiver.recv_from(&mut buf)?;
    assert_eq!(&buf[..n], b"AAA");     // Exactamente "AAA"

    let (n, _) = receiver.recv_from(&mut buf)?;
    assert_eq!(&buf[..n], b"BBBBB");   // Exactamente "BBBBB"

    let (n, _) = receiver.recv_from(&mut buf)?;
    assert_eq!(&buf[..n], b"CC");       // Exactamente "CC"

    println!("Datagram boundaries preserved!");
    Ok(())
}
```

### 3.3 Truncamiento de datagramas

Si el buffer de `recv_from` es menor que el datagrama, los datos excedentes se
**pierden** (truncados):

```rust
use std::net::UdpSocket;

fn main() -> std::io::Result<()> {
    let receiver = UdpSocket::bind("127.0.0.1:9000")?;
    let sender = UdpSocket::bind("127.0.0.1:0")?;

    // Enviar 100 bytes
    sender.send_to(&[0xAB; 100], "127.0.0.1:9000")?;

    // Recibir con buffer de solo 10 bytes
    let mut small_buf = [0u8; 10];
    let (n, _) = receiver.recv_from(&mut small_buf)?;

    println!("Received {} bytes (90 bytes LOST!)", n); // 10
    // Los otros 90 bytes se perdieron irrecuperablemente

    Ok(())
}
```

> **Regla**: siempre usar un buffer suficientemente grande. El datagrama UDP máximo
> teórico es 65535 bytes (header IP limita), pero en la práctica se recomienda
> no exceder ~1400 bytes (MTU de Ethernet - headers) para evitar fragmentación IP.

### 3.4 Tamaño máximo de datagramas

```
┌──────────────────────────┬──────────────────────┐
│ Límite                   │ Tamaño               │
├──────────────────────────┼──────────────────────┤
│ Teórico máximo           │ 65,535 bytes          │
│ (campo length de UDP)    │ (incluyendo header)   │
├──────────────────────────┼──────────────────────┤
│ Payload máximo           │ 65,507 bytes          │
│ (65535 - 20 IP - 8 UDP)  │                       │
├──────────────────────────┼──────────────────────┤
│ Seguro sin fragmentación │ ~1,472 bytes          │
│ (1500 MTU - 20 IP - 8 UDP)│ (Ethernet)          │
├──────────────────────────┼──────────────────────┤
│ Recomendado              │ ≤ 1,400 bytes         │
│ (margen para headers)    │                       │
└──────────────────────────┴──────────────────────┘

Si envías > 1472 bytes en Ethernet:
  → El paquete se FRAGMENTA a nivel IP
  → Si un fragmento se pierde, TODO el datagrama se pierde
  → La probabilidad de pérdida aumenta exponencialmente
```

---

## 4. `connect` y `send`/`recv`

### 4.1 UDP "conectado"

`UdpSocket::connect()` no establece una conexión real (no hay handshake). Solo
configura una dirección de destino por defecto, permitiendo usar `send()` y `recv()`
en vez de `send_to()` y `recv_from()`:

```rust
use std::net::UdpSocket;

fn main() -> std::io::Result<()> {
    let socket = UdpSocket::bind("127.0.0.1:0")?;

    // "Conectar" a un peer específico
    socket.connect("127.0.0.1:9000")?;

    // Ahora se puede usar send() sin especificar dirección
    socket.send(b"Hello")?;

    // recv() solo acepta datagramas del peer "conectado"
    // Datagramas de otros orígenes son descartados por el kernel
    let mut buf = [0u8; 1024];
    let n = socket.recv(&mut buf)?;
    println!("Received: {:?}", &buf[..n]);

    Ok(())
}
```

```
Sin connect:                    Con connect:
  send_to(data, addr)            connect(addr) ← una vez
  recv_from(&mut buf)             send(data)    ← más simple
  → acepta de cualquiera          recv(&mut buf)
                                  → solo del peer conectado

Ventajas de connect():
  1. API más simple (sin dirección en cada llamada)
  2. Filtrado por el kernel (solo recibe del peer)
  3. Errores ICMP se reportan (port unreachable)
  4. Ligeramente más rápido (el kernel cachea la ruta)
```

### 4.2 Errores ICMP con connect

Sin `connect`, si el peer no existe, `send_to` siempre tiene éxito (UDP es fire-and-forget).
Con `connect`, el kernel puede reportar errores ICMP:

```rust
use std::net::UdpSocket;

fn main() -> std::io::Result<()> {
    let socket = UdpSocket::bind("127.0.0.1:0")?;
    socket.connect("127.0.0.1:55555")?; // Puerto que nadie escucha

    // Primer send puede tener éxito (el error ICMP aún no llegó)
    socket.send(b"test")?;

    // En la siguiente operación, el error ICMP se reporta
    std::thread::sleep(std::time::Duration::from_millis(100));

    match socket.send(b"test2") {
        Ok(_) => println!("Sent (error may come later)"),
        Err(e) => println!("Error: {} (ICMP port unreachable)", e),
    }

    Ok(())
}
```

### 4.3 Cambiar el peer con otro `connect`

```rust
use std::net::UdpSocket;

fn main() -> std::io::Result<()> {
    let socket = UdpSocket::bind("127.0.0.1:0")?;

    // Conectar a un peer
    socket.connect("127.0.0.1:9000")?;
    socket.send(b"hello server A")?;

    // Cambiar de peer (no hay "desconexión", solo re-asociar)
    socket.connect("127.0.0.1:9001")?;
    socket.send(b"hello server B")?;

    // Se puede volver a usar send_to para otros destinos
    // (pero send() sigue usando el último connect)

    Ok(())
}
```

---

## 5. Broadcast

### 5.1 Qué es broadcast

Broadcast envía un datagrama a **todos** los hosts en la red local. Solo funciona
con UDP (TCP no soporta broadcast). La dirección de broadcast es típicamente
`255.255.255.255` (limited broadcast) o la dirección de broadcast de la subred
(ej: `192.168.1.255` para la red `192.168.1.0/24`).

```
Unicast:                          Broadcast:
  A ────────► B                     A ────────► B
                                    A ────────► C
                                    A ────────► D
                                    A ────────► E
                                    (todos en la red local)
```

### 5.2 Enviar broadcast

```rust
use std::net::UdpSocket;

fn main() -> std::io::Result<()> {
    let socket = UdpSocket::bind("0.0.0.0:0")?;

    // Habilitar broadcast (necesario antes de enviar)
    socket.set_broadcast(true)?;

    // Enviar a todos en la red local
    socket.send_to(
        b"DISCOVER: Who is there?",
        "255.255.255.255:9999", // O "192.168.1.255:9999"
    )?;

    println!("Broadcast sent");
    Ok(())
}
```

### 5.3 Recibir broadcast

```rust
use std::net::UdpSocket;

fn main() -> std::io::Result<()> {
    // Bind a 0.0.0.0 para recibir broadcasts
    let socket = UdpSocket::bind("0.0.0.0:9999")?;
    println!("Listening for broadcasts on :9999");

    let mut buf = [0u8; 1024];
    loop {
        let (n, src) = socket.recv_from(&mut buf)?;
        let msg = String::from_utf8_lossy(&buf[..n]);
        println!("From {}: {}", src, msg);
    }
}
```

### 5.4 Service discovery con broadcast

Patrón clásico: un cliente envía un broadcast preguntando "¿quién ofrece el
servicio X?", y los servidores responden con su dirección:

```rust
use std::net::UdpSocket;
use std::time::Duration;

const DISCOVERY_PORT: u16 = 9999;
const MAGIC: &[u8] = b"MYAPP_DISCOVER_v1";

fn discover_services(timeout: Duration) -> std::io::Result<Vec<std::net::SocketAddr>> {
    let socket = UdpSocket::bind("0.0.0.0:0")?;
    socket.set_broadcast(true)?;
    socket.set_read_timeout(Some(timeout))?;

    // Enviar pregunta
    socket.send_to(MAGIC, format!("255.255.255.255:{}", DISCOVERY_PORT))?;
    println!("Discovery broadcast sent...");

    // Recolectar respuestas durante el timeout
    let mut services = Vec::new();
    let mut buf = [0u8; 256];

    loop {
        match socket.recv_from(&mut buf) {
            Ok((n, src)) => {
                let response = String::from_utf8_lossy(&buf[..n]);
                println!("Found service at {}: {}", src, response);
                services.push(src);
            }
            Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock
                        || e.kind() == std::io::ErrorKind::TimedOut => {
                break; // Timeout alcanzado
            }
            Err(e) => return Err(e),
        }
    }

    Ok(services)
}

fn run_discoverable_service(service_port: u16) -> std::io::Result<()> {
    let socket = UdpSocket::bind(format!("0.0.0.0:{}", DISCOVERY_PORT))?;
    println!("Discovery listener on :{}", DISCOVERY_PORT);

    let mut buf = [0u8; 256];
    loop {
        let (n, src) = socket.recv_from(&mut buf)?;

        if &buf[..n] == MAGIC {
            let response = format!("SERVICE myapp PORT {}", service_port);
            socket.send_to(response.as_bytes(), src)?;
            println!("Responded to discovery from {}", src);
        }
    }
}
```

```
Discovery flow:

  Client                   Server A              Server B
  ──────                   ────────              ────────
    │                         │                     │
    ├── BROADCAST ──────────► │                     │
    │   "MYAPP_DISCOVER_v1"   ├── BROADCAST ──────► │
    │                         │                     │
    │ ◄── UNICAST ────────────┤                     │
    │    "SERVICE myapp       │                     │
    │     PORT 8080"          │                     │
    │                                               │
    │ ◄─────────────── UNICAST ─────────────────────┤
    │               "SERVICE myapp PORT 8081"       │
    │                                               │
  Ahora el cliente sabe:
    Server A: 192.168.1.10:8080
    Server B: 192.168.1.20:8081
```

---

## 6. Multicast

Multicast es como broadcast pero selectivo: solo los hosts que se **suscribieron**
a un grupo multicast reciben los datagramas. Usa direcciones IP en el rango
`224.0.0.0` - `239.255.255.255`.

### 6.1 Unirse a un grupo multicast

```rust
use std::net::{UdpSocket, Ipv4Addr};

fn main() -> std::io::Result<()> {
    let socket = UdpSocket::bind("0.0.0.0:5000")?;

    // Unirse al grupo multicast 239.1.2.3
    let multicast_addr = Ipv4Addr::new(239, 1, 2, 3);
    let interface = Ipv4Addr::UNSPECIFIED; // Cualquier interfaz
    socket.join_multicast_v4(&multicast_addr, &interface)?;

    println!("Joined multicast group 239.1.2.3 on :5000");

    let mut buf = [0u8; 1024];
    loop {
        let (n, src) = socket.recv_from(&mut buf)?;
        let msg = String::from_utf8_lossy(&buf[..n]);
        println!("Multicast from {}: {}", src, msg);
    }
}
```

### 6.2 Enviar a un grupo multicast

```rust
use std::net::UdpSocket;

fn main() -> std::io::Result<()> {
    let socket = UdpSocket::bind("0.0.0.0:0")?;

    // Opcionalmente: configurar TTL del multicast
    // TTL=1: solo red local (default)
    // TTL>1: puede cruzar routers
    socket.set_multicast_ttl_v4(1)?;

    // Enviar al grupo multicast
    socket.send_to(
        b"Hello multicast group!",
        "239.1.2.3:5000",
    )?;

    println!("Sent to multicast group 239.1.2.3:5000");
    Ok(())
}
```

### 6.3 Broadcast vs Multicast

```
Broadcast:
  ┌──────────────────────────────────┐
  │ Red local (192.168.1.0/24)       │
  │  A ──broadcast──► B ✓           │
  │                   C ✓ (recibe    │
  │                   D ✓  aunque no │
  │                   E ✓  quiera)   │
  └──────────────────────────────────┘
  → TODOS en la red reciben (spam de red)

Multicast grupo 239.1.2.3:
  ┌──────────────────────────────────┐
  │ Red local                        │
  │  A ──multicast──► B ✓ (suscrito) │
  │                   C ✗ (no susc.) │
  │                   D ✓ (suscrito) │
  │                   E ✗ (no susc.) │
  └──────────────────────────────────┘
  → Solo los suscriptores reciben

Multicast ventajas:
  - No interrumpe hosts no interesados
  - Puede cruzar routers (con IGMP)
  - Múltiples grupos independientes
```

### 6.4 Rangos de direcciones multicast

```
┌─────────────────────┬──────────────────────────────────┐
│ Rango               │ Uso                              │
├─────────────────────┼──────────────────────────────────┤
│ 224.0.0.0/24        │ Local (no forwarded por routers) │
│ 224.0.0.1           │ All Hosts (como broadcast)       │
│ 224.0.0.251         │ mDNS (Bonjour/Avahi)             │
│ 239.0.0.0/8         │ Administratively scoped (privado)│
│                     │ Usar este rango para apps propias│
└─────────────────────┴──────────────────────────────────┘
```

---

## 7. Patrones prácticos

### 7.1 Echo server UDP

```rust
use std::net::UdpSocket;

fn main() -> std::io::Result<()> {
    let socket = UdpSocket::bind("127.0.0.1:7777")?;
    println!("UDP echo server on :7777");

    let mut buf = [0u8; 65535];
    loop {
        let (n, src) = socket.recv_from(&mut buf)?;
        println!("[{}] {} bytes", src, n);

        // Responder al remitente
        socket.send_to(&buf[..n], src)?;
    }
}
```

### 7.2 Request-response con retry

UDP no garantiza entrega. Para request-response confiable, necesitas retry:

```rust
use std::net::UdpSocket;
use std::time::Duration;

fn udp_request(
    socket: &UdpSocket,
    server: &str,
    request: &[u8],
    max_retries: u32,
    timeout: Duration,
) -> std::io::Result<Vec<u8>> {
    socket.set_read_timeout(Some(timeout))?;

    for attempt in 0..=max_retries {
        if attempt > 0 {
            eprintln!("Retry {} of {}", attempt, max_retries);
        }

        // Enviar request
        socket.send_to(request, server)?;

        // Esperar respuesta
        let mut buf = [0u8; 65535];
        match socket.recv_from(&mut buf) {
            Ok((n, src)) => {
                // Verificar que la respuesta viene del servidor correcto
                let expected: std::net::SocketAddr = server.parse().unwrap();
                if src == expected {
                    return Ok(buf[..n].to_vec());
                }
                // Respuesta de otro origen — ignorar y reintentar
                eprintln!("Unexpected source: {}", src);
            }
            Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock
                        || e.kind() == std::io::ErrorKind::TimedOut => {
                // Timeout — reintentar
                continue;
            }
            Err(e) => return Err(e),
        }
    }

    Err(std::io::Error::new(
        std::io::ErrorKind::TimedOut,
        format!("No response after {} attempts", max_retries + 1),
    ))
}

fn main() -> std::io::Result<()> {
    let socket = UdpSocket::bind("127.0.0.1:0")?;

    match udp_request(
        &socket,
        "127.0.0.1:7777",
        b"PING",
        3,
        Duration::from_secs(2),
    ) {
        Ok(response) => {
            println!("Response: {:?}", String::from_utf8_lossy(&response));
        }
        Err(e) => println!("Failed: {}", e),
    }

    Ok(())
}
```

### 7.3 Simple DNS lookup (protocolo real)

Ejemplo simplificado de cómo funciona una consulta DNS real sobre UDP:

```rust
use std::net::UdpSocket;

fn build_dns_query(domain: &str) -> Vec<u8> {
    let mut packet = Vec::new();

    // Header (12 bytes)
    packet.extend_from_slice(&[
        0xAA, 0xBB,       // Transaction ID
        0x01, 0x00,       // Flags: standard query, recursion desired
        0x00, 0x01,       // Questions: 1
        0x00, 0x00,       // Answers: 0
        0x00, 0x00,       // Authority: 0
        0x00, 0x00,       // Additional: 0
    ]);

    // Question section: encode domain name
    for label in domain.split('.') {
        packet.push(label.len() as u8);
        packet.extend_from_slice(label.as_bytes());
    }
    packet.push(0x00); // Null terminator

    packet.extend_from_slice(&[
        0x00, 0x01, // Type: A (IPv4 address)
        0x00, 0x01, // Class: IN (Internet)
    ]);

    packet
}

fn main() -> std::io::Result<()> {
    let socket = UdpSocket::bind("0.0.0.0:0")?;
    socket.set_read_timeout(Some(std::time::Duration::from_secs(5)))?;

    let query = build_dns_query("example.com");

    // Enviar query al DNS resolver del sistema
    socket.send_to(&query, "8.8.8.8:53")?;

    let mut buf = [0u8; 512];
    let (n, _) = socket.recv_from(&mut buf)?;

    println!("DNS response: {} bytes", n);

    // Parsear respuesta (simplificado: extraer la IP del primer A record)
    if n > 12 {
        let answer_count = u16::from_be_bytes([buf[6], buf[7]]);
        println!("Answer count: {}", answer_count);

        // La IP está en los últimos 4 bytes de un A record
        if n >= 4 && answer_count > 0 {
            let ip = &buf[n-4..n];
            println!("IP: {}.{}.{}.{}", ip[0], ip[1], ip[2], ip[3]);
        }
    }

    Ok(())
}
```

### 7.4 Heartbeat / health check

```rust
use std::net::UdpSocket;
use std::time::{Duration, Instant};
use std::collections::HashMap;

fn heartbeat_monitor(port: u16, timeout: Duration) -> std::io::Result<()> {
    let socket = UdpSocket::bind(format!("0.0.0.0:{}", port))?;
    socket.set_read_timeout(Some(Duration::from_secs(1)))?;

    let mut last_seen: HashMap<std::net::SocketAddr, Instant> = HashMap::new();

    println!("Heartbeat monitor on :{}", port);

    loop {
        let mut buf = [0u8; 256];
        match socket.recv_from(&mut buf) {
            Ok((n, src)) => {
                let msg = String::from_utf8_lossy(&buf[..n]);
                let is_new = !last_seen.contains_key(&src);
                last_seen.insert(src, Instant::now());

                if is_new {
                    println!("[NEW] {} registered: {}", src, msg);
                }
            }
            Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {}
            Err(e) => eprintln!("Recv error: {}", e),
        }

        // Verificar timeouts
        let now = Instant::now();
        last_seen.retain(|addr, last| {
            if now.duration_since(*last) > timeout {
                println!("[DOWN] {} — no heartbeat for {:?}", addr, timeout);
                false
            } else {
                true
            }
        });
    }
}
```

---

## 8. Errores comunes

### Error 1: No habilitar broadcast antes de enviar

```rust
use std::net::UdpSocket;

// MAL — error "Permission denied" o silenciosamente ignorado
fn bad_broadcast() -> std::io::Result<()> {
    let socket = UdpSocket::bind("0.0.0.0:0")?;
    socket.send_to(b"hello", "255.255.255.255:9999")?;
    // Error: Os { code: 13, kind: PermissionDenied }
    Ok(())
}

// BIEN — habilitar broadcast primero
fn good_broadcast() -> std::io::Result<()> {
    let socket = UdpSocket::bind("0.0.0.0:0")?;
    socket.set_broadcast(true)?; // ← Necesario
    socket.send_to(b"hello", "255.255.255.255:9999")?;
    Ok(())
}
```

**Por qué ocurre**: por seguridad, el kernel requiere que el socket tenga
`SO_BROADCAST` habilitado antes de enviar a una dirección de broadcast. Esto
previene broadcasts accidentales.

### Error 2: Buffer demasiado pequeño (truncamiento silencioso)

```rust
use std::net::UdpSocket;

// MAL — si el datagrama es > 64 bytes, datos perdidos para siempre
fn bad_recv(socket: &UdpSocket) -> std::io::Result<()> {
    let mut buf = [0u8; 64]; // Demasiado pequeño
    let (n, _) = socket.recv_from(&mut buf)?;
    // Si el datagrama era 1000 bytes, solo recibimos 64
    // Los otros 936 están perdidos IRRECUPERABLEMENTE
    println!("Got {} bytes", n); // 64
    Ok(())
}

// BIEN — buffer del tamaño máximo esperado
fn good_recv(socket: &UdpSocket) -> std::io::Result<()> {
    let mut buf = [0u8; 65535]; // Max UDP
    let (n, src) = socket.recv_from(&mut buf)?;
    // n contiene el tamaño real del datagrama
    process(&buf[..n]);
    Ok(())
}

fn process(data: &[u8]) {
    println!("Got {} bytes", data.len());
}
```

### Error 3: Asumir que `send_to` garantiza la entrega

```rust
use std::net::UdpSocket;

// MAL — asumir que send_to exitoso = datos recibidos
fn bad_fire_and_forget(socket: &UdpSocket) -> std::io::Result<()> {
    socket.send_to(b"important data", "10.0.0.1:9000")?;
    println!("Data delivered!"); // ← FALSO
    // send_to exitoso solo significa: el kernel aceptó el datagrama
    // Puede perderse en la red, el peer puede estar muerto, etc.
    Ok(())
}

// BIEN — implementar confirmación si la entrega importa
fn reliable_send(
    socket: &UdpSocket,
    data: &[u8],
    dest: &str,
) -> std::io::Result<bool> {
    socket.set_read_timeout(Some(std::time::Duration::from_secs(2)))?;

    for attempt in 0..3 {
        socket.send_to(data, dest)?;

        let mut buf = [0u8; 4];
        match socket.recv_from(&mut buf) {
            Ok((n, _)) if &buf[..n] == b"ACK" => return Ok(true),
            Ok(_) => continue, // Respuesta inesperada
            Err(_) => {
                eprintln!("Attempt {} timed out", attempt + 1);
            }
        }
    }

    Ok(false)
}
```

### Error 4: Confundir `recv_from` con `recv` sin `connect`

```rust
use std::net::UdpSocket;

// MAL — recv() sin connect() previo
fn bad() -> std::io::Result<()> {
    let socket = UdpSocket::bind("127.0.0.1:9000")?;
    let mut buf = [0u8; 1024];

    // recv() sin connect() → error o comportamiento indefinido
    // (en la práctica, funciona pero no se sabe de quién viene el datagrama)
    let n = socket.recv(&mut buf)?;
    // ¿De quién era? No lo sabemos.
    println!("{} bytes from... somewhere", n);
    Ok(())
}

// BIEN — usar recv_from si no hiciste connect
fn good() -> std::io::Result<()> {
    let socket = UdpSocket::bind("127.0.0.1:9000")?;
    let mut buf = [0u8; 1024];

    let (n, src) = socket.recv_from(&mut buf)?;
    println!("{} bytes from {}", n, src);
    // Ahora sabemos de quién viene y podemos responder
    socket.send_to(b"ACK", src)?;
    Ok(())
}
```

### Error 5: Enviar datagramas más grandes que el MTU

```rust
use std::net::UdpSocket;

// MAL — datagrama de 10KB, será fragmentado
fn bad_large_send(socket: &UdpSocket) -> std::io::Result<()> {
    let large_data = vec![0u8; 10_000]; // 10 KB
    socket.send_to(&large_data, "10.0.0.1:9000")?;
    // Se fragmenta en ~7 paquetes IP
    // Si uno se pierde, todo el datagrama se pierde
    // Probabilidad de pérdida: 1 - (1-p)^7 ≈ 7p para p pequeño
    Ok(())
}

// BIEN — fragmentar en la aplicación
fn good_chunked_send(
    socket: &UdpSocket,
    data: &[u8],
    dest: &str,
    chunk_size: usize,
) -> std::io::Result<()> {
    let total_chunks = (data.len() + chunk_size - 1) / chunk_size;

    for (i, chunk) in data.chunks(chunk_size).enumerate() {
        // Header: [chunk_index: u16][total_chunks: u16][data...]
        let mut packet = Vec::with_capacity(4 + chunk.len());
        packet.extend_from_slice(&(i as u16).to_be_bytes());
        packet.extend_from_slice(&(total_chunks as u16).to_be_bytes());
        packet.extend_from_slice(chunk);

        socket.send_to(&packet, dest)?;
    }
    Ok(())
}
```

---

## 9. Cheatsheet

```text
──────────────────── UdpSocket ───────────────────────
UdpSocket::bind(addr)?             Crear y bind (server o client)
socket.send_to(data, addr)?        Enviar datagrama a addr
socket.recv_from(&mut buf)?        Recibir → (n_bytes, source_addr)
socket.connect(addr)?              Asociar peer por defecto
socket.send(data)?                 Enviar al peer (req connect)
socket.recv(&mut buf)?             Recibir del peer (req connect)

──────────────────── Opciones ────────────────────────
socket.set_broadcast(true)?        Habilitar broadcast
socket.set_read_timeout(Some(d))?  Timeout de lectura
socket.set_write_timeout(Some(d))? Timeout de escritura
socket.set_nonblocking(true)?      Modo non-blocking
socket.set_ttl(n)?                 IP TTL
socket.set_multicast_ttl_v4(n)?    TTL para multicast
socket.local_addr()?               Dirección local
socket.peer_addr()?                Peer (requiere connect)

──────────────────── Broadcast ───────────────────────
socket.set_broadcast(true)?        OBLIGATORIO antes de enviar
"255.255.255.255:port"             Limited broadcast
"192.168.1.255:port"               Directed broadcast (subred)
bind("0.0.0.0:port")               Necesario para RECIBIR broadcast

──────────────────── Multicast ───────────────────────
socket.join_multicast_v4(g, i)?    Unirse a grupo (g=grupo, i=interfaz)
socket.leave_multicast_v4(g, i)?   Salir del grupo
socket.set_multicast_ttl_v4(n)?    TTL (1=local, >1=cross-router)
socket.set_multicast_loop_v4(b)?   Recibir propio multicast
Rango: 224.0.0.0 — 239.255.255.255 Direcciones multicast
239.x.x.x                          Uso privado (recomendado)

──────────────────── Tamaños ─────────────────────────
Max datagrama teórico:  65,535 bytes (campo length UDP)
Max payload:            65,507 bytes (65535 - 20 IP - 8 UDP)
Seguro sin fragmentar:  ~1,472 bytes (1500 MTU - headers)
Recomendado:            ≤ 1,400 bytes

──────────────────── UDP vs TCP ──────────────────────
Entrega:                No garantizada (best-effort)
Orden:                  No garantizado
Boundaries:             Preservadas (1 send = 1 recv)
Conexión:               Sin conexión (datagramas)
Overhead:               Bajo (8 bytes header)
Broadcast/Multicast:    Sí
Truncamiento:           Datos excedentes PERDIDOS
```

---

## 10. Ejercicios

### Ejercicio 1: Chat UDP por broadcast

Implementa un chat simple donde todos los participantes en la red local pueden enviar
y recibir mensajes via broadcast:

```rust
use std::net::UdpSocket;
use std::thread;
use std::io::{self, BufRead};

const CHAT_PORT: u16 = 9999;

fn main() -> io::Result<()> {
    let username = std::env::args().nth(1)
        .unwrap_or_else(|| "anonymous".into());

    // TODO: Crear dos sockets:
    //   1. Sender: bind a 0.0.0.0:0, habilitar broadcast
    //   2. Receiver: bind a 0.0.0.0:CHAT_PORT

    // TODO: Thread de recepción:
    //   - Loop infinito con recv_from
    //   - Formato del mensaje: "username: text"
    //   - Ignorar mensajes propios (comparar addr local)
    //   - Imprimir: "[username] text"

    // TODO: Thread principal (o loop):
    //   - Leer líneas de stdin
    //   - Enviar como broadcast a 255.255.255.255:CHAT_PORT
    //   - Formato: "username: text\n"
    //   - "quit" para salir

    // Para probar: ejecutar en dos terminales diferentes

    todo!()
}
```

**Pregunta de reflexión**: ¿por qué necesitas dos sockets separados (uno para enviar y
otro para recibir) en vez de uno solo? ¿Qué pasa si usas un solo socket bound a
`0.0.0.0:9999` tanto para `send_to` como para `recv_from`? (Hint: depende del OS —
en Linux funciona, pero en otros puede no recibir sus propios broadcasts).

### Ejercicio 2: DNS resolver simplificado

Implementa un resolver DNS básico que envíe queries A-record y parsee la respuesta:

```rust
use std::net::UdpSocket;

fn resolve(domain: &str, dns_server: &str) -> std::io::Result<Vec<std::net::Ipv4Addr>> {
    // TODO:
    // 1. Construir un query DNS válido:
    //    - Header: ID aleatorio, flags=0x0100 (std query + RD),
    //      qdcount=1, ancount=0, nscount=0, arcount=0
    //    - Question: domain encodificado como labels
    //      (ej: "www.example.com" → [3]www[7]example[3]com[0])
    //      Type=A (0x0001), Class=IN (0x0001)
    //
    // 2. Enviar al dns_server:53 via UDP
    //
    // 3. Recibir respuesta con timeout de 5s (retry hasta 3 veces)
    //
    // 4. Parsear la respuesta:
    //    - Verificar que el ID coincide
    //    - Leer ancount (número de respuestas)
    //    - Saltar la question section
    //    - Para cada answer:
    //      - Saltar el name (puede ser compressed pointer 0xC0XX)
    //      - Leer type (2 bytes), class (2 bytes), ttl (4 bytes),
    //        rdlength (2 bytes), rdata (rdlength bytes)
    //      - Si type=A, rdata son 4 bytes = IPv4
    //
    // 5. Retornar las IPs encontradas

    todo!()
}

fn main() -> std::io::Result<()> {
    let domain = std::env::args().nth(1)
        .unwrap_or_else(|| "example.com".into());

    let dns = std::env::args().nth(2)
        .unwrap_or_else(|| "8.8.8.8".into());

    println!("Resolving {} via {}...", domain, dns);

    let ips = resolve(&domain, &dns)?;
    for ip in &ips {
        println!("  {} → {}", domain, ip);
    }

    if ips.is_empty() {
        println!("  No A records found");
    }

    Ok(())
}
```

**Pregunta de reflexión**: DNS usa UDP por defecto (puerto 53) para consultas cortas.
¿En qué situaciones DNS cae de vuelta a TCP? (Hint: respuestas mayores a 512 bytes,
zone transfers). ¿Por qué no usar TCP siempre — qué costo tiene el handshake de TCP
comparado con un solo round-trip UDP?

### Ejercicio 3: Reliable UDP con ACK

Implementa una capa de confiabilidad mínima sobre UDP: enviar con confirmación y retry:

```rust
use std::net::UdpSocket;
use std::time::Duration;

// Formato de paquete:
// [1 byte: tipo][4 bytes: sequence][payload...]
// Tipos: DATA=0x01, ACK=0x02

const DATA: u8 = 0x01;
const ACK: u8 = 0x02;

fn reliable_send(
    socket: &UdpSocket,
    dest: &str,
    data: &[u8],
    max_retries: u32,
    timeout: Duration,
) -> std::io::Result<()> {
    // TODO:
    // 1. Dividir data en chunks de 1024 bytes
    // 2. Para cada chunk:
    //    a. Construir paquete: [DATA][seq_number][chunk_data]
    //    b. Enviar el paquete
    //    c. Esperar ACK con el mismo seq_number
    //    d. Si timeout, reintentar hasta max_retries
    //    e. Si todos los retries fallan, retornar error
    // 3. Enviar paquete final: [DATA][seq=0xFFFFFFFF][] (vacío = fin)

    todo!()
}

fn reliable_recv(
    socket: &UdpSocket,
) -> std::io::Result<(Vec<u8>, std::net::SocketAddr)> {
    // TODO:
    // 1. Loop: recibir paquetes DATA
    // 2. Para cada paquete:
    //    a. Extraer seq_number y datos
    //    b. Si seq=0xFFFFFFFF → transmisión completa
    //    c. Enviar ACK con el mismo seq_number
    //    d. Almacenar datos (en orden por seq_number)
    //    e. Manejar duplicados (mismo seq → reenviar ACK pero no duplicar datos)
    // 3. Retornar datos completos ensamblados

    todo!()
}

fn main() -> std::io::Result<()> {
    // TODO: Probar enviando un archivo de prueba:
    //   Proceso 1 (receiver): reliable_recv en :5000
    //   Proceso 2 (sender): reliable_send a 127.0.0.1:5000
    //   Verificar que los datos recibidos == enviados

    Ok(())
}
```

**Pregunta de reflexión**: si esta implementación es básicamente TCP sobre UDP, ¿por qué
existen protocolos como QUIC que hacen exactamente esto? ¿Qué ventajas tiene implementar
confiabilidad en userspace vs usar TCP del kernel? (Hint: multiplexing sin head-of-line
blocking, 0-RTT handshake, migración de conexión).