# WebSocket

## Índice
1. [El problema: comunicación en tiempo real](#el-problema-comunicación-en-tiempo-real)
2. [Qué es WebSocket](#qué-es-websocket)
3. [El handshake de upgrade](#el-handshake-de-upgrade)
4. [Frames WebSocket](#frames-websocket)
5. [Comunicación bidireccional](#comunicación-bidireccional)
6. [Subprotocolos](#subprotocolos)
7. [Control de conexión: ping, pong y close](#control-de-conexión-ping-pong-y-close)
8. [WebSocket vs alternativas](#websocket-vs-alternativas)
9. [Seguridad](#seguridad)
10. [WebSocket detrás de reverse proxy](#websocket-detrás-de-reverse-proxy)
11. [Errores comunes](#errores-comunes)
12. [Cheatsheet](#cheatsheet)
13. [Ejercicios](#ejercicios)

---

## El problema: comunicación en tiempo real

HTTP sigue un modelo request/response estricto: el cliente pregunta, el servidor responde. El servidor **nunca** puede enviar datos al cliente por iniciativa propia. Esto es un problema para aplicaciones que necesitan actualizaciones en tiempo real:

```
┌──────────────────────────────────────────────────────────────┐
│          Workarounds antes de WebSocket                      │
│                                                              │
│  1. POLLING (el cliente pregunta repetidamente)              │
│                                                              │
│     Cliente: ¿hay datos nuevos?     → Servidor: No           │
│     [espera 2 seg]                                           │
│     Cliente: ¿hay datos nuevos?     → Servidor: No           │
│     [espera 2 seg]                                           │
│     Cliente: ¿hay datos nuevos?     → Servidor: ¡Sí! {data} │
│                                                              │
│     Problema: peticiones inútiles, latencia de hasta 2 seg,  │
│     carga innecesaria en el servidor.                        │
│                                                              │
│  2. LONG POLLING (Comet)                                     │
│                                                              │
│     Cliente: ¿hay datos nuevos?                              │
│     Servidor: [mantiene la conexión abierta... esperando...] │
│     Servidor: ¡Sí! {data} → cierra respuesta                │
│     Cliente: ¿hay datos nuevos? (reconecta inmediatamente)   │
│                                                              │
│     Mejor latencia, pero: una conexión TCP por cliente       │
│     esperando, overhead de cabeceras HTTP en cada reconexión.│
│                                                              │
│  3. SERVER-SENT EVENTS (SSE)                                 │
│                                                              │
│     Cliente: GET /events (Accept: text/event-stream)         │
│     Servidor: [mantiene conexión abierta, envía eventos]     │
│       data: {"price": 42.50}\n\n                             │
│       data: {"price": 42.75}\n\n                             │
│       ...                                                    │
│                                                              │
│     Unidireccional: servidor → cliente solamente.            │
│     Funciona bien para notificaciones, pero no para chat.    │
└──────────────────────────────────────────────────────────────┘
```

---

## Qué es WebSocket

WebSocket (RFC 6455) es un protocolo que proporciona comunicación **bidireccional full-duplex** sobre una sola conexión TCP. Ambos lados pueden enviar datos en cualquier momento, sin esperar una petición del otro:

```
┌──────────────────────────────────────────────────────────────┐
│                HTTP vs WebSocket                             │
│                                                              │
│  HTTP:                                                       │
│  ┌────────┐  request    ┌────────┐                           │
│  │Cliente │ ──────────► │Servidor│                           │
│  │        │ ◄────────── │        │  ← solo puede responder   │
│  └────────┘  response   └────────┘    a una petición         │
│                                                              │
│  WebSocket:                                                  │
│  ┌────────┐             ┌────────┐                           │
│  │Cliente │ ◄──────────►│Servidor│                           │
│  │        │  mensajes   │        │  ← ambos envían cuando    │
│  │        │  en ambas   │        │    quieran, sin esperar   │
│  │        │  direcciones│        │                           │
│  └────────┘             └────────┘                           │
│                                                              │
│  Características:                                            │
│  • Full-duplex: ambos lados envían simultáneamente           │
│  • Baja latencia: sin overhead de cabeceras HTTP por mensaje │
│  • Persistente: la conexión se mantiene abierta              │
│  • Basado en mensajes (no bytes como TCP raw)                │
│  • Protocolo propio (ws:// o wss://) sobre TCP               │
└──────────────────────────────────────────────────────────────┘
```

Casos de uso ideales:

| Caso | ¿Por qué WebSocket? |
|------|---------------------|
| Chat en tiempo real | Ambos lados envían mensajes en cualquier momento |
| Juegos multijugador | Latencia mínima, actualizaciones frecuentes bidireccionales |
| Dashboards en vivo | El servidor pushea métricas/datos continuamente |
| Editores colaborativos | Cambios de múltiples usuarios sincronizados al instante |
| Trading/cotizaciones | Precios actualizados decenas de veces por segundo |
| Terminales remotas | stdin/stdout bidireccional en tiempo real |

---

## El handshake de upgrade

WebSocket comienza como una petición HTTP/1.1 normal y luego "upgrade" a WebSocket. Este mecanismo permite que WebSocket pase por proxies HTTP y funcione en el puerto 80/443:

```
┌──────────────────────────────────────────────────────────────┐
│              WebSocket Handshake                             │
│                                                              │
│  Cliente → Servidor:                                         │
│  ─────────────────────────────────────────────────           │
│  GET /chat HTTP/1.1                                          │
│  Host: server.example.com                                    │
│  Upgrade: websocket                    ← quiero upgrade      │
│  Connection: Upgrade                   ← cabecera hop-by-hop │
│  Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==  ← nonce      │
│  Sec-WebSocket-Version: 13             ← versión del proto   │
│  Sec-WebSocket-Protocol: chat, json    ← subprotocolos       │
│  Origin: https://app.example.com       ← para validación     │
│                                                              │
│  Servidor → Cliente:                                         │
│  ─────────────────────────────────────────────────           │
│  HTTP/1.1 101 Switching Protocols                            │
│  Upgrade: websocket                                          │
│  Connection: Upgrade                                         │
│  Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=         │
│  Sec-WebSocket-Protocol: json          ← subprotocolo elegido│
│                                                              │
│  ── A partir de aquí, la conexión TCP es WebSocket ──        │
│  ── Ya no se habla HTTP ──                                   │
└──────────────────────────────────────────────────────────────┘
```

### Sec-WebSocket-Key y Sec-WebSocket-Accept

Este intercambio no es autenticación — es una verificación de que el servidor entiende WebSocket y no es un servidor HTTP normal que acepta cualquier cosa:

```
┌──────────────────────────────────────────────────────────────┐
│  Cálculo de Sec-WebSocket-Accept:                            │
│                                                              │
│  1. Tomar Sec-WebSocket-Key del cliente:                     │
│     "dGhlIHNhbXBsZSBub25jZQ=="                               │
│                                                              │
│  2. Concatenar con el GUID mágico (fijo por especificación): │
│     "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"                   │
│                                                              │
│  3. SHA-1 del resultado → Base64                             │
│     = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="                        │
│                                                              │
│  4. El servidor envía este valor en Sec-WebSocket-Accept     │
│                                                              │
│  Si el valor no coincide, el cliente rechaza la conexión.    │
│  Propósito: confirmar que el servidor habla WebSocket,       │
│  no que accidentalmente aceptó el Upgrade.                   │
└──────────────────────────────────────────────────────────────┘
```

### ¿Por qué HTTP/1.1 y no HTTP/2?

El mecanismo `Upgrade` es exclusivo de HTTP/1.1. HTTP/2 tiene su propia extensión para WebSocket (RFC 8441, "Bootstrapping WebSockets with HTTP/2") que usa CONNECT en vez de Upgrade. Sin embargo, en la práctica la gran mayoría de implementaciones WebSocket siguen usando el handshake HTTP/1.1.

### ws:// y wss://

```
ws://server.example.com/chat       ← sin cifrar (puerto 80)
wss://server.example.com/chat      ← con TLS (puerto 443)

# wss:// es el equivalente de https:// para WebSocket
# En producción, SIEMPRE usar wss://
```

---

## Frames WebSocket

Después del handshake, la comunicación se realiza mediante **frames** (no confundir con frames HTTP/2 — son protocolos distintos):

```
┌──────────────────────────────────────────────────────────────┐
│                  Frame WebSocket                             │
│                                                              │
│   0                   1                   2                  │
│   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3          │
│  +-+-+-+-+-------+-+-------------+-------------------------------+
│  |F|R|R|R| opcode|M| Payload len |  Extended payload length      │
│  |I|S|S|S|  (4)  |A|     (7)     |        (16/64 bits)           │
│  |N|V|V|V|       |S|             |                               │
│  | |1|2|3|       |K|             |                               │
│  +-+-+-+-+-------+-+-------------+-------------------------------+
│  | Masking-key (0 or 4 bytes)                                    │
│  +---------------------------------------------------------------+
│  | Payload Data                                                  │
│  +---------------------------------------------------------------+
│                                                              │
│  FIN (1 bit): 1 = último frame del mensaje                   │
│               0 = continuará en más frames (fragmentación)   │
│                                                              │
│  Opcode (4 bits):                                            │
│    0x0 = Continuation frame                                  │
│    0x1 = Text frame (UTF-8)                                  │
│    0x2 = Binary frame                                        │
│    0x8 = Close                                               │
│    0x9 = Ping                                                │
│    0xA = Pong                                                │
│                                                              │
│  MASK (1 bit): 1 = payload está enmascarado                  │
│    Los frames del CLIENTE siempre llevan mask = 1            │
│    Los frames del SERVIDOR siempre llevan mask = 0           │
│                                                              │
│  Payload length:                                             │
│    0-125: tamaño real                                        │
│    126: los siguientes 2 bytes indican el tamaño (16-bit)    │
│    127: los siguientes 8 bytes indican el tamaño (64-bit)    │
└──────────────────────────────────────────────────────────────┘
```

### Overhead mínimo

Comparado con HTTP, el overhead de WebSocket por mensaje es mínimo:

```
┌──────────────────────────────────────────────────────────────┐
│           Overhead por mensaje                               │
│                                                              │
│  HTTP (cada petición/respuesta):                             │
│    ~200-800 bytes de cabeceras                               │
│    (Host, User-Agent, Accept, Cookie, Content-Type...)       │
│                                                              │
│  WebSocket:                                                  │
│    2 bytes   (mensajes ≤ 125 bytes, sin mask)                │
│    6 bytes   (mensajes ≤ 125 bytes, con mask)                │
│    4 bytes   (mensajes ≤ 65535 bytes, sin mask)              │
│    8 bytes   (mensajes ≤ 65535 bytes, con mask)              │
│                                                              │
│  Para un mensaje de chat de 50 bytes:                        │
│    HTTP:      ~300 bytes overhead → 600% overhead            │
│    WebSocket: 6 bytes overhead    → 12% overhead             │
└──────────────────────────────────────────────────────────────┘
```

### Tipos de datos: text vs binary

```
┌───────────────────────────────────────────────────────────┐
│  Text frame (opcode 0x1):                                 │
│    Payload DEBE ser UTF-8 válido.                         │
│    Típico para JSON, XML, texto plano.                    │
│                                                           │
│  Binary frame (opcode 0x2):                               │
│    Payload es bytes arbitrarios.                          │
│    Típico para protobuf, imágenes, audio/video,           │
│    formatos binarios propios.                             │
│                                                           │
│  Fragmentación (FIN=0 + opcode continuation):             │
│    Un mensaje grande puede dividirse en múltiples frames.  │
│    Solo el primer frame tiene el opcode real (text/binary).│
│    Los siguientes tienen opcode 0x0 (continuation).       │
│    El último tiene FIN=1.                                 │
│                                                           │
│  Frame 1: FIN=0, opcode=0x1, "Hola "                     │
│  Frame 2: FIN=0, opcode=0x0, "mundo "                     │
│  Frame 3: FIN=1, opcode=0x0, "cruel"                      │
│  → Mensaje completo: "Hola mundo cruel"                   │
└───────────────────────────────────────────────────────────┘
```

### Masking: ¿por qué el cliente enmascara?

```
┌──────────────────────────────────────────────────────────────┐
│  ¿Por qué los frames del cliente llevan mask?                │
│                                                              │
│  NO es para cifrado (XOR con clave conocida no cifra nada). │
│                                                              │
│  Es para proteger contra cache poisoning en proxies HTTP     │
│  que no entienden WebSocket:                                 │
│                                                              │
│  Sin mask, un atacante podría craftar un payload que se      │
│  parezca a una respuesta HTTP válida. Un proxy intermedio    │
│  que no entienda WebSocket podría cachearlo como HTTP.       │
│                                                              │
│  El masking XOR hace que el payload sea impredecible para    │
│  proxies que no entienden el protocolo, previniendo que      │
│  lo interpreten como HTTP cacheable.                         │
│                                                              │
│  Los frames del servidor NO se enmascaran porque el          │
│  ataque solo es viable en dirección cliente → proxy.         │
└──────────────────────────────────────────────────────────────┘
```

---

## Comunicación bidireccional

Una vez establecida la conexión, ambos lados envían frames de forma independiente y asíncrona:

```
┌──────────────────────────────────────────────────────────────┐
│            Ejemplo: chat en tiempo real                      │
│                                                              │
│  Tiempo  Cliente                    Servidor                 │
│   │                                                          │
│   │  ──[text: "hola"]──────────►                             │
│   │                                                          │
│   │  ◄──[text: "hola a todos"]──  (broadcast a otro usuario)│
│   │                                                          │
│   │  ◄──[text: "¿qué tal?"]────  (otro usuario escribió)    │
│   │                                                          │
│   │  ──[text: "bien!"]─────────►                             │
│   │                                                          │
│   │  ◄──[ping]─────────────────  (keepalive del servidor)    │
│   │  ──[pong]──────────────────►                             │
│   │                                                          │
│   │  ◄──[text: "hasta luego"]──  (otro usuario)              │
│   │                                                          │
│   │  ──[close: 1000]──────────►  (cierre normal)             │
│   │  ◄──[close: 1000]─────────  (confirmación)               │
│   ▼                                                          │
│   Conexión TCP cerrada                                       │
└──────────────────────────────────────────────────────────────┘
```

No hay concepto de "request" ni "response" — solo mensajes. La aplicación define el significado de cada mensaje (típicamente mediante JSON con un campo `type`):

```json
// Mensaje del cliente:
{"type": "chat_message", "room": "general", "text": "Hola"}

// Mensaje del servidor:
{"type": "chat_message", "from": "Ana", "room": "general", "text": "Hola"}

// Notificación del servidor:
{"type": "user_joined", "user": "Carlos", "room": "general"}

// Evento del servidor:
{"type": "typing", "user": "Ana", "room": "general"}
```

---

## Subprotocolos

WebSocket es un transporte genérico. Los **subprotocolos** definen el formato y las reglas de los mensajes que viajan por encima:

```
┌──────────────────────────────────────────────────────────────┐
│  Handshake con subprotocolos:                                │
│                                                              │
│  Cliente:                                                    │
│  Sec-WebSocket-Protocol: graphql-ws, json, mqtt              │
│  "Soporto estos subprotocolos, elige uno"                    │
│                                                              │
│  Servidor:                                                   │
│  Sec-WebSocket-Protocol: graphql-ws                          │
│  "Usaremos graphql-ws"                                       │
│                                                              │
│  Si el servidor no soporta ninguno, puede:                   │
│  • Omitir la cabecera (conexión sin subprotocolo)            │
│  • Rechazar la conexión (no responder 101)                   │
└──────────────────────────────────────────────────────────────┘
```

Subprotocolos registrados en IANA:

| Subprotocolo | Uso |
|-------------|-----|
| `graphql-ws` / `graphql-transport-ws` | Suscripciones GraphQL |
| `mqtt` | IoT messaging (MQTT sobre WebSocket) |
| `wamp` | Web Application Messaging Protocol (RPC + pub/sub) |
| `soap` | SOAP sobre WebSocket |
| `stomp` | Simple Text Oriented Messaging Protocol |

---

## Control de conexión: ping, pong y close

### Ping/Pong (keepalive)

```
┌──────────────────────────────────────────────────────────────┐
│  Ping (opcode 0x9):                                          │
│  • Cualquier lado puede enviar un ping                       │
│  • Puede contener payload (datos de aplicación, ≤125 bytes)  │
│  • El receptor DEBE responder con pong                       │
│                                                              │
│  Pong (opcode 0xA):                                          │
│  • Respuesta obligatoria a un ping                           │
│  • Debe incluir el mismo payload que el ping                 │
│  • Un pong no solicitado (sin ping previo) se ignora         │
│                                                              │
│  Uso típico del servidor:                                    │
│    Enviar ping cada 30 segundos.                             │
│    Si no recibe pong en 10 segundos → cerrar conexión.       │
│    Detecta clientes desconectados (red caída, app crasheada).│
│                                                              │
│  Servidor: ──[ping]──►                                       │
│  Cliente:  ◄──[pong]──  (automático, lo maneja el framework) │
│                                                              │
│  Si la red del cliente cae silenciosamente (sin TCP RST):    │
│  Servidor: ──[ping]──► (no llega)                            │
│  Servidor: [espera 10s... timeout] → cierra conexión         │
└──────────────────────────────────────────────────────────────┘
```

### Close (cierre ordenado)

```
┌──────────────────────────────────────────────────────────────┐
│  Close (opcode 0x8):                                         │
│                                                              │
│  1. Cualquier lado envía close frame                         │
│  2. El otro lado responde con su propio close frame          │
│  3. Tras enviar close, no se envían más data frames          │
│  4. Tras recibir close response, se cierra TCP               │
│                                                              │
│  Payload del close frame:                                    │
│  ┌────────────────────────────────────┐                      │
│  │ Status code (2 bytes) │ Reason    │                      │
│  │ (opcional)            │ (UTF-8)   │                      │
│  └────────────────────────────────────┘                      │
│                                                              │
│  Códigos de cierre:                                          │
│    1000  Normal closure (cierre limpio)                       │
│    1001  Going Away (servidor se reinicia, cliente navega)    │
│    1002  Protocol Error                                      │
│    1003  Unsupported Data (ej. binario cuando espera texto)  │
│    1006  Abnormal Closure (sin close frame — conexión rota)  │
│    1008  Policy Violation                                    │
│    1009  Message Too Big                                     │
│    1011  Unexpected Condition (error interno del servidor)    │
│    1012  Service Restart                                     │
│    1013  Try Again Later (sobrecarga temporal)                │
│                                                              │
│  1005 y 1006 NUNCA se envían en un close frame — son         │
│  códigos internos que indican "no hubo close frame".         │
└──────────────────────────────────────────────────────────────┘
```

---

## WebSocket vs alternativas

```
┌────────────────────┬────────────────┬────────────────┬───────────────┐
│                    │   WebSocket    │    SSE          │  Long Polling │
├────────────────────┼────────────────┼────────────────┼───────────────┤
│ Dirección          │ Bidireccional  │ Server→Client  │ Server→Client │
│ Protocolo          │ ws:// / wss:// │ HTTP           │ HTTP          │
│ Reconexión auto    │ No (manual)    │ Sí (built-in)  │ Manual        │
│ Overhead/mensaje   │ 2-8 bytes      │ ~20-50 bytes   │ ~300-800 bytes│
│ Datos binarios     │ Sí             │ No (solo texto)│ Sí (con base64)│
│ HTTP/2 compatible  │ Con RFC 8441   │ Sí, nativo     │ Sí            │
│ Firewall friendly  │ Puede fallar   │ Siempre pasa   │ Siempre pasa  │
│ Escalabilidad      │ Conexión/user  │ Conexión/user  │ Conexión/user │
│ Complejidad server │ Alta           │ Baja           │ Media         │
└────────────────────┴────────────────┴────────────────┴───────────────┘
```

### Cuándo usar cada uno

```
┌──────────────────────────────────────────────────────────────┐
│                   Árbol de decisión                          │
│                                                              │
│  ¿Necesitas enviar datos del cliente al servidor             │
│   frecuentemente (no solo la petición inicial)?              │
│     │                                                        │
│     ├── Sí → ¿Necesitas latencia <100ms?                     │
│     │        │                                               │
│     │        ├── Sí → WebSocket                              │
│     │        │        (chat, juegos, terminales)              │
│     │        │                                               │
│     │        └── No → HTTP (request/response normal)         │
│     │                 (formularios, CRUD API)                 │
│     │                                                        │
│     └── No → ¿Solo el servidor pushea datos?                 │
│              │                                               │
│              ├── Sí → SSE (Server-Sent Events)               │
│              │        (notificaciones, feeds, dashboards)     │
│              │                                               │
│              └── No → HTTP normal con polling                │
│                       (status checks poco frecuentes)        │
│                                                              │
│  Regla general:                                              │
│  • Si puedes resolverlo con HTTP normal, hazlo.              │
│  • Si necesitas server push unidireccional, usa SSE.         │
│  • Solo usa WebSocket cuando realmente necesitas             │
│    comunicación bidireccional de baja latencia.              │
└──────────────────────────────────────────────────────────────┘
```

### SSE en detalle (la alternativa más subestimada)

```bash
# Petición SSE:
GET /events HTTP/1.1
Accept: text/event-stream

# Respuesta (la conexión permanece abierta):
HTTP/1.1 200 OK
Content-Type: text/event-stream
Cache-Control: no-cache
Connection: keep-alive

event: price_update
data: {"symbol": "BTC", "price": 67250.00}

event: price_update
data: {"symbol": "ETH", "price": 3450.00}

event: alert
data: {"message": "BTC subió 5% en 1h"}

# Ventajas de SSE sobre WebSocket:
# • Reconexión automática con Last-Event-ID
# • Funciona perfectamente con HTTP/2 multiplexing
# • Pasa cualquier firewall/proxy (es HTTP normal)
# • Mucho más simple de implementar en el servidor
```

### WebSocket vs QUIC streams

```
┌──────────────────────────────────────────────────────────────┐
│  En HTTP/3, los QUIC streams ya son bidireccionales.         │
│                                                              │
│  ¿WebSocket se vuelve obsoleto con HTTP/3?                   │
│  No exactamente:                                             │
│                                                              │
│  • HTTP/3 streams son request/response (semántica HTTP)      │
│  • WebSocket es un protocolo de mensajería libre              │
│  • WebRTC DataChannel es otra alternativa (P2P)              │
│                                                              │
│  Pero WebTransport (W3C/IETF draft) podría reemplazar        │
│  WebSocket eventualmente: usa QUIC streams directamente,     │
│  soporta bidireccional, unidireccional, y unreliable.       │
│  Todavía en desarrollo.                                      │
└──────────────────────────────────────────────────────────────┘
```

---

## Seguridad

### Validación de Origin

```
┌──────────────────────────────────────────────────────────────┐
│  Durante el handshake, el servidor recibe:                   │
│  Origin: https://app.example.com                             │
│                                                              │
│  El servidor DEBE validar este Origin.                       │
│  Si no lo valida, cualquier sitio web puede conectarse:      │
│                                                              │
│  1. Usuario logueado en app.example.com                      │
│  2. Visita evil.com                                          │
│  3. evil.com abre: new WebSocket("wss://app.example.com/ws") │
│  4. El navegador envía cookies de app.example.com            │
│  5. Si el servidor no valida Origin → CSRF via WebSocket     │
│                                                              │
│  Mitigación:                                                 │
│  • Validar Origin en el handshake                            │
│  • Rechazar orígenes no esperados (no enviar 101)            │
│  • Usar tokens de autenticación además de cookies            │
└──────────────────────────────────────────────────────────────┘
```

### Autenticación

WebSocket no tiene un mecanismo de autenticación propio. Estrategias comunes:

```
┌──────────────────────────────────────────────────────────────┐
│  1. Cookie-based (si ws:// está en el mismo dominio):        │
│     Las cookies se envían automáticamente en el handshake.   │
│     El servidor las valida antes de aceptar el upgrade.      │
│                                                              │
│  2. Token en query string:                                   │
│     new WebSocket("wss://server.com/ws?token=eyJhb...")      │
│     ⚠ El token aparece en logs del servidor y del proxy.    │
│     Aceptable solo si el token es de corta duración.         │
│                                                              │
│  3. Token en primer mensaje:                                 │
│     ws.onopen = () => {                                      │
│       ws.send(JSON.stringify({type: "auth", token: "eyJ..."}))│
│     }                                                        │
│     El servidor no envía datos hasta recibir auth válida.    │
│     Más limpio que query string.                             │
│                                                              │
│  4. Ticket/token de un solo uso:                             │
│     GET /api/ws-ticket → {"ticket": "abc123"}                │
│     new WebSocket("wss://server.com/ws?ticket=abc123")       │
│     El servidor valida y consume el ticket (1 uso).          │
│     Más seguro: el ticket no se puede reutilizar.            │
└──────────────────────────────────────────────────────────────┘
```

### wss:// obligatorio en producción

```
# ws:// (sin TLS) tiene los mismos problemas que http://:
# • Los datos viajan en texto claro
# • Vulnerable a man-in-the-middle
# • Proxies pueden inspeccionar/modificar frames
# • Algunos firewalls bloquean ws:// pero permiten wss://
#   (porque wss:// se ve como tráfico HTTPS normal)
#
# SIEMPRE usar wss:// en producción.
```

---

## WebSocket detrás de reverse proxy

Configurar WebSocket detrás de nginx o HAProxy requiere configuración específica porque el mecanismo `Upgrade` necesita propagarse:

### nginx

```nginx
# Configuración para WebSocket en nginx
upstream websocket_backend {
    server 127.0.0.1:3000;
}

server {
    listen 443 ssl;
    server_name app.example.com;

    ssl_certificate /etc/ssl/cert.pem;
    ssl_certificate_key /etc/ssl/key.pem;

    # Rutas HTTP normales
    location / {
        proxy_pass http://websocket_backend;
    }

    # Ruta WebSocket
    location /ws {
        proxy_pass http://websocket_backend;

        # Estas 3 líneas son ESENCIALES para WebSocket:
        proxy_http_version 1.1;                    # debe ser 1.1
        proxy_set_header Upgrade $http_upgrade;     # propagar Upgrade
        proxy_set_header Connection "upgrade";      # propagar Connection

        # Headers adicionales útiles:
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        # Timeouts para conexiones de larga duración:
        proxy_read_timeout 86400s;    # 24h (default: 60s)
        proxy_send_timeout 86400s;
    }
}
```

> **`proxy_read_timeout`**: por defecto nginx cierra conexiones después de 60 segundos de inactividad. WebSocket necesita un timeout mucho mayor. Combinado con ping/pong a nivel aplicación, `86400s` es una configuración segura.

### ¿Por qué `proxy_http_version 1.1`?

```
┌──────────────────────────────────────────────────────────────┐
│  nginx por defecto usa HTTP/1.0 para conectar al backend.    │
│  HTTP/1.0 NO soporta el mecanismo Upgrade.                   │
│                                                              │
│  Cliente ──[HTTP/1.1 Upgrade]──► nginx ──[HTTP/1.0]──► backend
│                                          ↑                   │
│                                     La cabecera Upgrade      │
│                                     se pierde con HTTP/1.0   │
│                                                              │
│  Con proxy_http_version 1.1:                                 │
│  Cliente ──[HTTP/1.1 Upgrade]──► nginx ──[HTTP/1.1]──► backend
│                                          ↑                   │
│                                     Upgrade se propaga       │
│                                     correctamente            │
└──────────────────────────────────────────────────────────────┘
```

---

## Errores comunes

### 1. No implementar reconexión automática en el cliente

```
# WebSocket NO reconecta automáticamente (a diferencia de SSE).
# Si la red cae, el WebSocket muere y no vuelve.

# INCORRECTO: abrir una sola vez
const ws = new WebSocket("wss://server.com/ws");

# CORRECTO: reconexión con backoff exponencial
function connect() {
    const ws = new WebSocket("wss://server.com/ws");
    let retryDelay = 1000;  // 1 segundo inicial

    ws.onclose = () => {
        setTimeout(() => {
            retryDelay = Math.min(retryDelay * 2, 30000); // max 30s
            connect();
        }, retryDelay);
    };

    ws.onopen = () => {
        retryDelay = 1000;  // reset al conectar
    };
}
```

### 2. Usar WebSocket cuando SSE es suficiente

```
# Si tu aplicación solo necesita:
#   Servidor → Cliente (notificaciones, feeds, métricas)
#
# SSE es más simple, más robusto, y funciona mejor con HTTP/2:
# • Reconexión automática (built-in)
# • Funciona con cualquier proxy HTTP
# • Complejidad de servidor: mínima
# • Compatible con HTTP/2 multiplexing
#
# WebSocket solo se justifica cuando necesitas:
# • Bidireccional frecuente (chat, juegos)
# • Datos binarios
# • Latencia sub-100ms en ambas direcciones
```

### 3. No configurar proxy_read_timeout en nginx

```nginx
# DEFAULT: proxy_read_timeout 60s;
# → nginx cierra la conexión WebSocket tras 60 segundos
#   de inactividad (sin datos ni ping/pong)

# Síntoma: los WebSocket se desconectan "misteriosamente"
# cada minuto

# Solución A: aumentar el timeout
proxy_read_timeout 86400s;

# Solución B: implementar ping/pong cada <60 segundos
# (la aplicación envía ping cada 30s, lo que resetea el timer)
# Mejor combinar ambas soluciones.
```

### 4. Enviar el token de autenticación en la URL sin precauciones

```
# PROBLEMA: el token aparece en logs
wss://server.com/ws?token=eyJhbGciOiJIUzI1NiJ9.eyJ1c2VyIjoiYWRtaW4ifQ...

# → Log de nginx: "GET /ws?token=eyJ... HTTP/1.1" 101
# → Cualquiera con acceso a los logs ve el token

# MEJOR: token de un solo uso (ticket)
# 1. POST /api/ws-ticket → {"ticket": "random-uuid-here"}
# 2. wss://server.com/ws?ticket=random-uuid-here
#    (el ticket se invalida tras el primer uso)
```

### 5. No validar Origin en el servidor

```
# Un servidor WebSocket sin validación de Origin acepta
# conexiones de CUALQUIER sitio web.
#
# CORS no aplica a WebSocket — es el servidor quien
# debe validar la cabecera Origin manualmente.
#
# Si tu servidor acepta conexiones sin verificar Origin:
# → Cualquier página web puede conectar al WebSocket
# → Si hay cookies de sesión, se envían automáticamente
# → CSRF via WebSocket
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│                  WebSocket Cheatsheet                         │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  CONCEPTO:                                                   │
│    Comunicación bidireccional full-duplex sobre TCP           │
│    Comienza con HTTP/1.1 Upgrade, luego protocolo propio     │
│    ws:// (puerto 80) y wss:// (puerto 443, con TLS)          │
│                                                              │
│  HANDSHAKE:                                                  │
│    Cliente: GET /ws + Upgrade: websocket + Sec-WebSocket-Key │
│    Servidor: 101 Switching Protocols + Sec-WebSocket-Accept  │
│    Después del 101 → ya no es HTTP                           │
│                                                              │
│  FRAMES:                                                     │
│    Text (0x1)      datos UTF-8 (JSON, texto)                 │
│    Binary (0x2)    datos arbitrarios (protobuf, imágenes)    │
│    Close (0x8)     cierre ordenado + código + razón           │
│    Ping (0x9)      keepalive (receptor debe enviar pong)     │
│    Pong (0xA)      respuesta a ping                          │
│    Continuation (0x0)  fragmentación de mensajes grandes     │
│                                                              │
│  OVERHEAD:                                                   │
│    2-8 bytes por frame (vs ~300-800 bytes HTTP)              │
│    Mask obligatorio: cliente→servidor (4 bytes extra)        │
│    Mask prohibido: servidor→cliente                          │
│                                                              │
│  CÓDIGOS DE CIERRE:                                          │
│    1000  Normal        1001  Going Away                      │
│    1002  Protocol Err  1003  Unsupported Data                │
│    1006  Abnormal (no enviable — indica ausencia de close)   │
│    1008  Policy Viol.  1009  Too Big                         │
│    1011  Internal Err  1012  Service Restart                 │
│                                                              │
│  SEGURIDAD:                                                  │
│    SIEMPRE wss:// en producción                              │
│    Validar Origin en el servidor (CORS no aplica)            │
│    Token en primer mensaje > token en query string           │
│    Ticket de un solo uso: lo más seguro                      │
│                                                              │
│  NGINX PROXY (3 líneas esenciales):                          │
│    proxy_http_version 1.1;                                   │
│    proxy_set_header Upgrade $http_upgrade;                   │
│    proxy_set_header Connection "upgrade";                    │
│    proxy_read_timeout 86400s;  ← evitar corte a los 60s     │
│                                                              │
│  CUÁNDO USAR:                                                │
│    WebSocket:   bidireccional, baja latencia (chat, juegos)  │
│    SSE:         server→client only (notif., feeds, dashboards)│
│    HTTP normal:  request/response (CRUD, formularios)        │
│                                                              │
│  RECONEXIÓN:                                                 │
│    WebSocket NO reconecta automáticamente                    │
│    Implementar backoff exponencial: 1s, 2s, 4s... max 30s   │
│    SSE SÍ reconecta automáticamente (ventaja clave)          │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Analizar un handshake WebSocket

Dada esta captura de tráfico:

```
> GET /live-data HTTP/1.1
> Host: stream.example.com
> Upgrade: websocket
> Connection: Upgrade
> Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==
> Sec-WebSocket-Version: 13
> Sec-WebSocket-Protocol: json, protobuf
> Origin: https://dashboard.example.com
> Cookie: session=abc123

< HTTP/1.1 101 Switching Protocols
< Upgrade: websocket
< Connection: Upgrade
< Sec-WebSocket-Accept: HSmrc0sMlYUkAGmm5OPpG2HaGWk=
< Sec-WebSocket-Protocol: json
```

**Responde**:
1. ¿Qué subprotocolos ofreció el cliente? ¿Cuál eligió el servidor?
2. ¿Desde qué página web se inició esta conexión WebSocket?
3. Si el servidor devolviera `HTTP/1.1 403 Forbidden` en vez de 101, ¿qué significaría? ¿Cuál podría ser la causa?
4. Después del 101, ¿se pueden enviar cabeceras HTTP adicionales?
5. ¿El envío de `Cookie: session=abc123` es automático? ¿Qué riesgo tiene si el servidor no valida `Origin`?

**Pregunta de reflexión**: Este handshake usa HTTP/1.1. Si el servidor también soporta HTTP/2, ¿el navegador usaría HTTP/2 para el handshake WebSocket? ¿Qué mecanismo alternativo existe (pista: RFC 8441)?

---

### Ejercicio 2: Elegir la tecnología correcta

Para cada escenario, elige entre **WebSocket**, **SSE**, **HTTP polling**, o **HTTP normal**, y justifica tu elección:

1. Un dashboard que muestra métricas de servidores actualizadas cada 5 segundos. El usuario solo observa, no interactúa con los datos.

2. Una aplicación de chat grupal donde los usuarios envían y reciben mensajes constantemente.

3. Una página que muestra el estado de un deployment CI/CD (pending → running → success/failed). Se consulta cada 30 segundos.

4. Un editor colaborativo donde múltiples usuarios editan un documento simultáneamente y ven los cambios de los demás en tiempo real.

5. Un sistema de notificaciones push (nuevos emails, alertas) donde el usuario puede marcar notificaciones como leídas.

6. Un juego de ajedrez online por turnos.

**Pregunta de reflexión**: En el escenario 5, ¿por qué sería incorrecto usar WebSocket para marcar notificaciones como leídas, y qué usarías en su lugar? ¿Es posible combinar SSE (para recibir) con HTTP normal (para enviar)?

---

### Ejercicio 3: Configurar nginx para WebSocket

Tienes un servidor WebSocket escuchando en `127.0.0.1:8080` y nginx como reverse proxy con TLS. Los usuarios reportan que los WebSocket se desconectan "aleatoriamente" cada minuto.

**Parte A**: Escribe la configuración nginx que:
1. Escuche en el puerto 443 con TLS.
2. Envíe tráfico HTTP normal a `127.0.0.1:3000`.
3. Envíe tráfico WebSocket (ruta `/ws`) a `127.0.0.1:8080`.
4. Propague correctamente las cabeceras Upgrade y Connection.
5. Resuelva el problema de desconexión a los 60 segundos.
6. Pase la IP real del cliente al backend.

**Parte B**: Después de aplicar tu configuración, un usuario reporta que el WebSocket conecta pero se desconecta inmediatamente con código 1006. Al revisar los logs de nginx ves:

```
upstream prematurely closed connection while reading response header from upstream
```

¿Qué está pasando? ¿Dónde buscarías el problema?

**Pregunta de reflexión**: ¿Por qué nginx necesita `proxy_http_version 1.1` específicamente para WebSocket? ¿Qué versión HTTP usa nginx por defecto para comunicarse con el backend y por qué esa versión es incompatible con Upgrade?

---

> **Siguiente tema**: C02/S01/T01 — Jerarquía DNS (root servers, TLDs, dominios, FQDN)
