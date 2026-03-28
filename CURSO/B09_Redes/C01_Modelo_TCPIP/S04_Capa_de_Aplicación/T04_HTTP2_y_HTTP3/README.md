# HTTP/2 y HTTP/3

## Índice
1. [Los problemas de HTTP/1.1](#los-problemas-de-http11)
2. [HTTP/2: visión general](#http2-visión-general)
3. [Binary framing](#binary-framing)
4. [Multiplexing](#multiplexing)
5. [Header compression (HPACK)](#header-compression-hpack)
6. [Server push](#server-push)
7. [Priorización de streams](#priorización-de-streams)
8. [HTTP/3 y QUIC](#http3-y-quic)
9. [QUIC en detalle](#quic-en-detalle)
10. [Comparativa práctica](#comparativa-práctica)
11. [Diagnóstico y herramientas](#diagnóstico-y-herramientas)
12. [Errores comunes](#errores-comunes)
13. [Cheatsheet](#cheatsheet)
14. [Ejercicios](#ejercicios)

---

## Los problemas de HTTP/1.1

HTTP/1.1 funciona bien, pero tiene limitaciones estructurales que se vuelven evidentes con las páginas web modernas (decenas de recursos por página):

```
┌──────────────────────────────────────────────────────────────┐
│           Problemas de HTTP/1.1                              │
│                                                              │
│  1. HEAD-OF-LINE BLOCKING                                    │
│     Las respuestas deben llegar en orden.                    │
│     Si la primera es lenta, bloquea a todas las demás.       │
│                                                              │
│     GET /slow-query  ────── esperando 3s ──── Resp           │
│     GET /fast.css    ────── bloqueado ──────── Resp           │
│     GET /tiny.js     ────── bloqueado ──────────── Resp      │
│                                                              │
│  2. MÚLTIPLES CONEXIONES TCP                                 │
│     Workaround: los navegadores abren 6 conexiones por       │
│     dominio. Cada una requiere TCP handshake + TLS.          │
│                                                              │
│     Conexión 1: GET /a ─── Resp                              │
│     Conexión 2: GET /b ─── Resp                              │
│     Conexión 3: GET /c ─── Resp    ← 6 handshakes = costoso │
│     ...                                                      │
│                                                              │
│  3. CABECERAS REDUNDANTES                                    │
│     Cada petición envía las mismas cabeceras (Cookie,        │
│     User-Agent, Accept...). En 100 peticiones, se repiten    │
│     los mismos ~800 bytes 100 veces = ~80 KB desperdiciados. │
│                                                              │
│  4. SOLO PULL                                                │
│     El servidor no puede enviar recursos que sabe que el     │
│     cliente necesitará — debe esperar a que los pida.        │
└──────────────────────────────────────────────────────────────┘
```

Optimizaciones de HTTP/1.1 que son workarounds de estas limitaciones:

| Técnica | Problema que resuelve | Efecto secundario |
|---------|----------------------|-------------------|
| Domain sharding | Límite de 6 conexiones por dominio | Más DNS lookups, más handshakes |
| CSS/JS bundling | Muchas peticiones pequeñas | Invalida toda la caché por un cambio mínimo |
| Image sprites | Muchas peticiones de imágenes | Complejidad de CSS, descarga todo para usar una |
| Inlining (data URIs) | Peticiones extra | No cacheable independientemente |

> **Dato clave**: todas estas técnicas son anti-patrones en HTTP/2. Con multiplexing, muchos archivos pequeños son más eficientes que uno grande porque la caché se invalida granularmente.

---

## HTTP/2: visión general

HTTP/2 (RFC 9113, originalmente RFC 7540) fue publicado en 2015, basado en el protocolo SPDY de Google. Su objetivo: resolver los problemas de rendimiento de HTTP/1.1 sin cambiar la semántica (mismos métodos, códigos, cabeceras).

```
┌──────────────────────────────────────────────────────────────┐
│                  HTTP/1.1 vs HTTP/2                           │
│                                                              │
│  Lo que CAMBIA:                                              │
│  • Formato: texto → binario                                  │
│  • Multiplexing: múltiples streams en 1 conexión TCP         │
│  • Compresión de cabeceras: HPACK                            │
│  • Server push: el servidor envía sin que le pidan           │
│  • Priorización de streams                                   │
│                                                              │
│  Lo que NO cambia:                                           │
│  • Métodos (GET, POST, PUT...)                               │
│  • Códigos de estado (200, 404, 500...)                      │
│  • Cabeceras (Host, Content-Type...)                         │
│  • Semántica de la petición/respuesta                        │
│  • URLs, cookies, autenticación                              │
│                                                              │
│  Una aplicación web NO necesita cambios de código para       │
│  funcionar sobre HTTP/2 — es transparente.                   │
└──────────────────────────────────────────────────────────────┘
```

### Negociación del protocolo

```
┌──────────────────────────────────────────────────────────────┐
│           ¿Cómo se elige HTTP/2?                             │
│                                                              │
│  Con TLS (lo habitual):                                      │
│    ALPN (Application-Layer Protocol Negotiation)             │
│    Durante el handshake TLS, el cliente envía una lista      │
│    de protocolos soportados: ["h2", "http/1.1"]              │
│    El servidor elige "h2" si lo soporta.                     │
│                                                              │
│  Sin TLS (raro, no soportado por navegadores):               │
│    HTTP Upgrade mechanism                                    │
│    GET / HTTP/1.1                                            │
│    Upgrade: h2c                                              │
│    → El servidor responde 101 Switching Protocols            │
│                                                              │
│  En la práctica: HTTP/2 ≈ siempre con TLS (h2)              │
│  Los navegadores NO soportan h2c (HTTP/2 sin TLS).           │
└──────────────────────────────────────────────────────────────┘
```

---

## Binary framing

La diferencia más fundamental entre HTTP/1.1 y HTTP/2 es el formato de transmisión:

```
┌──────────────────────────────────────────────────────────────┐
│  HTTP/1.1 (texto):                                           │
│  ──────────────────                                          │
│  GET / HTTP/1.1\r\n          ← legible por humanos           │
│  Host: example.com\r\n       ← parseo complejo               │
│  Accept: text/html\r\n       ← ambigüedad en delimitadores   │
│  \r\n                                                        │
│                                                              │
│  HTTP/2 (binario):                                           │
│  ──────────────────                                          │
│  ┌─────────────────────────────────────┐                     │
│  │ Frame header (9 bytes, fijo)       │                     │
│  │  Length (3B) │ Type (1B) │ Flags (1B) │ Stream ID (4B)   │
│  ├─────────────────────────────────────┤                     │
│  │ Frame payload (variable)           │                     │
│  │ [datos binarios]                   │                     │
│  └─────────────────────────────────────┘                     │
│                                                              │
│  Cada mensaje HTTP se descompone en FRAMES.                  │
│  Cada frame pertenece a un STREAM identificado por un ID.    │
└──────────────────────────────────────────────────────────────┘
```

### Jerarquía: conexión → stream → mensaje → frame

```
┌──────────────────────────────────────────────────────────────┐
│  CONEXIÓN TCP (una sola)                                     │
│  │                                                           │
│  ├── Stream 1 (request GET /index.html)                      │
│  │   ├── HEADERS frame  (cabeceras de la petición)           │
│  │   └── DATA frame     (body — vacío en GET)                │
│  │   ← HEADERS frame    (cabeceras de la respuesta)          │
│  │   ← DATA frame       (body: HTML)                         │
│  │   ← DATA frame       (continuación del body)              │
│  │                                                           │
│  ├── Stream 3 (request GET /style.css)                       │
│  │   ├── HEADERS frame                                       │
│  │   ← HEADERS frame                                         │
│  │   ← DATA frame (body: CSS)                                │
│  │                                                           │
│  ├── Stream 5 (request POST /api/data)                       │
│  │   ├── HEADERS frame                                       │
│  │   ├── DATA frame (body: JSON)                             │
│  │   ← HEADERS frame                                         │
│  │   ← DATA frame (body: respuesta JSON)                     │
│  │                                                           │
│  └── Stream 0 (control — SETTINGS, WINDOW_UPDATE, etc.)      │
└──────────────────────────────────────────────────────────────┘
```

### Tipos de frame

| Tipo | Código | Función |
|------|--------|---------|
| DATA | 0x0 | Transporta el body del mensaje |
| HEADERS | 0x1 | Cabeceras HTTP (comprimidas con HPACK) |
| PRIORITY | 0x2 | Prioridad del stream (deprecado en RFC 9113) |
| RST_STREAM | 0x3 | Cancelar un stream individual sin cerrar la conexión |
| SETTINGS | 0x4 | Configuración de la conexión (tamaño de ventana, max streams...) |
| PUSH_PROMISE | 0x5 | Server push: anuncia un recurso que el servidor enviará |
| PING | 0x6 | Verificar que la conexión sigue viva, medir RTT |
| GOAWAY | 0x7 | Cierre ordenado de la conexión |
| WINDOW_UPDATE | 0x8 | Control de flujo por stream o por conexión |
| CONTINUATION | 0x9 | Continuación de un bloque HEADERS que no cabe en un frame |

> **RST_STREAM** es muy poderoso: permite cancelar una petición individual sin afectar al resto de los streams ni cerrar la conexión TCP. En HTTP/1.1, cancelar una petición requería cerrar toda la conexión.

---

## Multiplexing

El cambio más impactante. HTTP/2 permite enviar y recibir múltiples peticiones/respuestas **simultáneamente** en una sola conexión TCP, sin head-of-line blocking a nivel HTTP:

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│  HTTP/1.1 (6 conexiones):         HTTP/2 (1 conexión):       │
│                                                              │
│  Conn 1: ──GET /a──Resp──         ──┬ HEADERS (stream 1)    │
│  Conn 2: ──GET /b──Resp──           │ HEADERS (stream 3)    │
│  Conn 3: ──GET /c──Resp──           │ DATA (stream 3) ←CSS  │
│  Conn 4: ──GET /d──Resp──           │ DATA (stream 1) ←HTML │
│  Conn 5: ──GET /e──Resp──           │ HEADERS (stream 5)    │
│  Conn 6: ──GET /f──Resp──           │ DATA (stream 5)       │
│                                     │ DATA (stream 1) cont. │
│  6 TCP handshakes                   │ DATA (stream 7)       │
│  6 TLS handshakes                   └──────────────────      │
│  6× memoria en el servidor          1 handshake total       │
│                                     Frames intercalados     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Cómo funciona el intercalado

Los frames de diferentes streams se intercalan libremente. El receptor los reagrupa por Stream ID:

```
┌────────────────────────────────────────────────────────────┐
│  Cable TCP (secuencia real de frames):                     │
│                                                            │
│  [H:s1][H:s3][D:s3][D:s1][H:s5][D:s5][D:s1][D:s3]       │
│   ───── ───── ───── ───── ───── ───── ───── ─────         │
│    │     │     │     │     │     │     │     │             │
│    │     │     │     │     │     │     │     └─ CSS cont.  │
│    │     │     │     │     │     │     └─ HTML cont.       │
│    │     │     │     │     │     └─ JS data                │
│    │     │     │     │     └─ JS headers                   │
│    │     │     │     └─ HTML data                          │
│    │     │     └─ CSS data                                 │
│    │     └─ CSS headers                                    │
│    └─ HTML headers                                         │
│                                                            │
│  H = HEADERS frame   D = DATA frame   s = stream          │
│  Receptor reagrupa: s1=[H,D,D]  s3=[H,D,D]  s5=[H,D]     │
└────────────────────────────────────────────────────────────┘
```

### Stream IDs

- Streams iniciados por el **cliente**: IDs impares (1, 3, 5, 7...).
- Streams iniciados por el **servidor** (push): IDs pares (2, 4, 6...).
- **Stream 0**: reservado para frames de control (SETTINGS, PING, GOAWAY).
- Los IDs nunca se reutilizan — si se agotan, se abre una nueva conexión.

### Control de flujo

HTTP/2 implementa control de flujo a **dos niveles**: por conexión y por stream individual:

```
┌──────────────────────────────────────────────────────────┐
│             Control de flujo HTTP/2                       │
│                                                          │
│  Ventana de conexión: 65535 bytes (default)               │
│  ├── Stream 1: ventana 65535 bytes                       │
│  ├── Stream 3: ventana 65535 bytes                       │
│  └── Stream 5: ventana 65535 bytes                       │
│                                                          │
│  Solo aplica a DATA frames (no a HEADERS).               │
│  WINDOW_UPDATE frame incrementa la ventana.              │
│                                                          │
│  Propósito: evitar que un stream lento o un receptor     │
│  lento consuma toda la memoria del buffer.               │
└──────────────────────────────────────────────────────────┘
```

### El problema que persiste: TCP head-of-line blocking

```
┌──────────────────────────────────────────────────────────────┐
│  HTTP/2 resuelve HOL blocking a nivel HTTP...                │
│  ...pero TCP todavía tiene su propio HOL blocking:           │
│                                                              │
│  TCP ve todos los frames como un flujo de bytes continuo.    │
│  Si se pierde un paquete TCP, TODOS los streams se bloquean  │
│  hasta que ese paquete se retransmite.                        │
│                                                              │
│  Paquete 1: [frame s1] [frame s3]    ← llega OK             │
│  Paquete 2: [frame s5] [frame s1]    ← SE PIERDE            │
│  Paquete 3: [frame s3] [frame s7]    ← llega, pero TCP      │
│                                         lo retiene hasta     │
│                                         que paquete 2 se     │
│                                         retransmita          │
│                                                              │
│  Streams 3 y 7 NO necesitan el paquete 2, pero TCP           │
│  los bloquea igualmente. Este problema motivó HTTP/3.        │
└──────────────────────────────────────────────────────────────┘
```

---

## Header compression (HPACK)

HTTP/1.1 envía las cabeceras como texto sin comprimir en cada petición. Con cookies grandes, esto puede ser cientos de bytes repetidos en cada request. HPACK resuelve esto:

```
┌──────────────────────────────────────────────────────────────┐
│                    HPACK: dos técnicas                        │
│                                                              │
│  1. TABLA ESTÁTICA (61 entradas predefinidas)                │
│     Cabeceras comunes tienen un índice numérico:             │
│                                                              │
│     Índice 2  → :method GET                                  │
│     Índice 3  → :method POST                                 │
│     Índice 4  → :path /                                      │
│     Índice 17 → accept-encoding gzip, deflate                │
│     ...                                                      │
│                                                              │
│     En lugar de enviar ":method: GET" (12 bytes),            │
│     se envía el índice 2 (1 byte).                           │
│                                                              │
│  2. TABLA DINÁMICA (por conexión)                            │
│     Cabeceras que se repiten entre peticiones                 │
│     se añaden a una tabla compartida:                        │
│                                                              │
│     Petición 1: Cookie: session=abc123  (envío completo)     │
│     → Se añade a tabla dinámica como índice 62               │
│     Petición 2: índice 62  (1 byte en vez de 25)             │
│     Petición 3: índice 62  (1 byte)                          │
│                                                              │
│  Resultado: ~85-90% de reducción en tamaño de cabeceras     │
│  tras las primeras peticiones.                               │
└──────────────────────────────────────────────────────────────┘
```

### Pseudo-headers

HTTP/2 convierte la request line de HTTP/1.1 en pseudo-headers (prefijadas con `:`):

```
HTTP/1.1:                          HTTP/2:
GET /path HTTP/1.1        →       :method = GET
Host: example.com                  :path = /path
                                   :scheme = https
                                   :authority = example.com

HTTP/1.1 200 OK           →       :status = 200
```

Las pseudo-headers **deben** ir antes de las cabeceras regulares.

---

## Server push

El servidor puede enviar recursos que anticipa que el cliente necesitará, antes de que los pida:

```
┌──────────────────────────────────────────────────────────────┐
│                    Server Push                               │
│                                                              │
│  Sin push (HTTP/1.1 o HTTP/2 sin push):                      │
│                                                              │
│  Cliente: GET /index.html                                    │
│  Servidor: 200 OK (HTML)                                     │
│  Cliente: [parsea HTML, descubre <link href="style.css">]    │
│  Cliente: GET /style.css              ← 1 RTT extra          │
│  Servidor: 200 OK (CSS)                                      │
│  Cliente: [parsea CSS, descubre url(font.woff2)]             │
│  Cliente: GET /font.woff2             ← otro RTT extra       │
│  Servidor: 200 OK (font)                                     │
│                                                              │
│  Con push:                                                   │
│                                                              │
│  Cliente: GET /index.html                                    │
│  Servidor: PUSH_PROMISE (stream 2: /style.css)               │
│  Servidor: PUSH_PROMISE (stream 4: /font.woff2)              │
│  Servidor: HEADERS + DATA stream 1 (HTML)                    │
│  Servidor: HEADERS + DATA stream 2 (CSS)     ← 0 RTTs extra │
│  Servidor: HEADERS + DATA stream 4 (font)    ← 0 RTTs extra │
│                                                              │
│  El cliente recibe HTML + CSS + font en 1 solo RTT.          │
└──────────────────────────────────────────────────────────────┘
```

### El declive de server push

A pesar de la elegancia del concepto, server push ha caído en desuso:

```
┌──────────────────────────────────────────────────────────────┐
│  Problemas prácticos de server push:                         │
│                                                              │
│  1. DESPERDICIA ANCHO DE BANDA                               │
│     Si el cliente ya tiene style.css en caché,               │
│     el servidor lo envía innecesariamente.                   │
│     RST_STREAM puede cancelar, pero ya consumió bytes.       │
│                                                              │
│  2. DIFÍCIL DE CONFIGURAR                                    │
│     ¿Qué recursos pushear? ¿En qué orden?                   │
│     Depende de la caché del cliente, que el servidor         │
│     no conoce con certeza.                                   │
│                                                              │
│  3. COMPITE CON ALTERNATIVAS MEJORES                         │
│     • <link rel="preload"> en HTML/cabeceras                 │
│     • 103 Early Hints (informational response)               │
│     Ambas permiten que el servidor "sugiera" recursos        │
│     sin el riesgo de enviarlos innecesariamente.             │
│                                                              │
│  Estado actual:                                              │
│  • Chrome eliminó soporte para server push (2022)            │
│  • HTTP/3 (RFC 9114) lo incluye pero su uso es mínimo       │
│  • La industria prefiere 103 Early Hints                     │
└──────────────────────────────────────────────────────────────┘
```

### 103 Early Hints: la alternativa

```
# El servidor envía una respuesta informacional ANTES
# de la respuesta final:

HTTP/2 103 Early Hints
Link: </style.css>; rel=preload; as=style
Link: </app.js>; rel=preload; as=script

HTTP/2 200 OK
Content-Type: text/html
...

# El navegador comienza a descargar style.css y app.js
# mientras espera que el servidor termine de generar el HTML.
# Sin riesgo de desperdiciar ancho de banda.
```

---

## Priorización de streams

HTTP/2 permite que el cliente indique qué recursos son más importantes:

```
┌──────────────────────────────────────────────────────────────┐
│           Priorización (modelo original de RFC 7540)         │
│                                                              │
│  Cada stream tiene:                                          │
│  • Peso: 1-256 (proporción de ancho de banda)                │
│  • Dependencia: puede depender de otro stream                │
│                                                              │
│              stream 0 (raíz)                                 │
│             /          \                                     │
│        stream 1       stream 3                               │
│       (HTML, w=256)  (CSS, w=256)                            │
│            |                                                 │
│        stream 5                                              │
│       (JS, w=128)                                            │
│                                                              │
│  → HTML y CSS reciben igual ancho de banda                   │
│  → JS solo se envía después de que HTML termine              │
│  → JS recibe la mitad del ancho de banda que HTML            │
│                                                              │
│  PROBLEMA: este modelo era demasiado complejo y los          │
│  servidores lo implementaban de forma inconsistente.         │
└──────────────────────────────────────────────────────────────┘
```

La RFC 9218 introdujo **Extensible Priorities**, un modelo más simple adoptado por HTTP/2 y HTTP/3:

```
# Cabecera Priority (RFC 9218):
Priority: u=0            ← urgencia 0 (máxima, para HTML)
Priority: u=2            ← urgencia 2 (para CSS)
Priority: u=3, i         ← urgencia 3, incremental (para imágenes)

# u = urgencia (0-7, menor = más urgente)
# i = incremental (el recurso es útil parcialmente,
#     como una imagen progresiva)
```

---

## HTTP/3 y QUIC

HTTP/3 (RFC 9114, 2022) reemplaza TCP con **QUIC** como protocolo de transporte, resolviendo el TCP head-of-line blocking:

```
┌──────────────────────────────────────────────────────────────┐
│                    Pila de protocolos                         │
│                                                              │
│  HTTP/1.1          HTTP/2            HTTP/3                  │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐               │
│  │ HTTP/1.1 │    │ HTTP/2   │    │ HTTP/3   │               │
│  ├──────────┤    ├──────────┤    ├──────────┤               │
│  │   TLS    │    │   TLS    │    │   QUIC   │ ← integra    │
│  ├──────────┤    ├──────────┤    │  (+ TLS  │   TLS 1.3    │
│  │   TCP    │    │   TCP    │    │   1.3)   │               │
│  ├──────────┤    ├──────────┤    ├──────────┤               │
│  │    IP    │    │    IP    │    │   UDP    │ ← sobre UDP   │
│  └──────────┘    └──────────┘    ├──────────┤               │
│                                  │    IP    │               │
│                                  └──────────┘               │
│                                                              │
│  TCP handshake: 1 RTT              QUIC: 0-1 RTT           │
│  + TLS 1.3: 1 RTT adicional       (TLS integrado)          │
│  Total: 2 RTTs                     Total: 1 RTT (o 0)      │
└──────────────────────────────────────────────────────────────┘
```

### ¿Por qué UDP?

QUIC no usa UDP porque sea "más rápido". Lo usa porque:

1. **TCP está osificado**: las middleboxes (firewalls, NATs, ISPs) inspeccionan y a veces modifican paquetes TCP. Desplegar un nuevo protocolo de transporte es prácticamente imposible — los routers lo descartarían.
2. **UDP pasa por todas partes**: es el protocolo de transporte más simple y los middleboxes lo dejan pasar sin inspeccionar su contenido.
3. **QUIC implementa fiabilidad encima de UDP**: reordenamiento, retransmisión, control de congestión — todo lo que hace TCP, pero de forma diferente.

```
┌──────────────────────────────────────────────────────────────┐
│  QUIC NO es "UDP sin fiabilidad". QUIC es un protocolo      │
│  de transporte completo que UTILIZA UDP como envoltorio      │
│  para atravesar la infraestructura de red existente.         │
│                                                              │
│  TCP = fiabilidad implementada en el kernel                  │
│  QUIC = fiabilidad implementada en espacio de usuario        │
│         (encapsulada en paquetes UDP)                         │
└──────────────────────────────────────────────────────────────┘
```

---

## QUIC en detalle

### Eliminación del TCP HOL blocking

El avance fundamental de QUIC: cada stream tiene su propio control de entrega independiente:

```
┌──────────────────────────────────────────────────────────────┐
│  HTTP/2 sobre TCP: HOL blocking                              │
│  ─────────────────────────────                               │
│  Paquete 1: [stream 1] [stream 3]    ← OK                   │
│  Paquete 2: [stream 5]              ← PERDIDO               │
│  Paquete 3: [stream 1] [stream 7]    ← llega, pero TCP      │
│             ↑                          retiene TODO hasta    │
│             stream 1 bloqueado         que paquete 2 se      │
│             stream 7 bloqueado         retransmite           │
│             (aunque no necesitan                             │
│              el paquete 2)                                   │
│                                                              │
│  HTTP/3 sobre QUIC: sin HOL blocking                         │
│  ──────────────────────────────────                          │
│  Paquete 1: [stream 1] [stream 3]    ← OK, entregados       │
│  Paquete 2: [stream 5]              ← PERDIDO               │
│  Paquete 3: [stream 1] [stream 7]    ← OK, entregados       │
│                                        inmediatamente        │
│             Solo stream 5 espera la retransmisión.           │
│             Streams 1, 3, 7 no se ven afectados.             │
└──────────────────────────────────────────────────────────────┘
```

### 0-RTT connection establishment

QUIC integra TLS 1.3 en el handshake, reduciendo la latencia inicial:

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│  TCP + TLS 1.3 (primera conexión):                           │
│                                                              │
│  Cliente                    Servidor                         │
│    │── SYN ──────────────────►│         ┐                    │
│    │◄── SYN+ACK ─────────────│         │ 1 RTT (TCP)        │
│    │── ACK ──────────────────►│         ┘                    │
│    │── ClientHello ──────────►│         ┐                    │
│    │◄── ServerHello + Cert ──│         │ 1 RTT (TLS)        │
│    │── Finished ─────────────►│         ┘                    │
│    │── GET /index.html ──────►│         ← datos: 2 RTTs     │
│                                                              │
│  QUIC (primera conexión):                                    │
│                                                              │
│  Cliente                    Servidor                         │
│    │── Initial (ClientHello) ►│         ┐ 1 RTT             │
│    │◄── Initial (ServerHello)─│         │ (handshake +       │
│    │── Handshake Done ────────►│         ┘  datos juntos)    │
│    │── GET /index.html ──────►│         ← datos: 1 RTT      │
│                                                              │
│  QUIC (reconexión con 0-RTT):                                │
│                                                              │
│  Cliente                    Servidor                         │
│    │── Initial + 0-RTT data ─►│         ← datos: 0 RTTs     │
│    │   (ClientHello + GET /)  │         (sesión resumida)    │
│    │◄── respuesta ───────────│                               │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

> **Seguridad de 0-RTT**: los datos enviados en 0-RTT no están protegidos contra **replay attacks**. Por eso, solo peticiones idempotentes (GET) deberían usar 0-RTT. Los servidores pueden rechazar 0-RTT data y forzar un handshake completo de 1-RTT.

### Connection migration

Con TCP, una conexión se identifica por la 4-tupla (IP origen, puerto origen, IP destino, puerto destino). Si cambias de red (WiFi → móvil), la IP cambia y la conexión TCP muere.

```
┌──────────────────────────────────────────────────────────────┐
│                Connection Migration                          │
│                                                              │
│  TCP (HTTP/2):                                               │
│    WiFi: 192.168.1.50:52431 → server:443  (conexión A)      │
│    [cambia a 4G]                                             │
│    4G:   10.0.0.15:48920 → server:443     (conexión B)      │
│    → Conexión A muere. Nuevo TCP + TLS handshake.            │
│    → Todas las peticiones en progreso se pierden.            │
│                                                              │
│  QUIC (HTTP/3):                                              │
│    WiFi: 192.168.1.50:52431 → server:443  (Connection ID: X)│
│    [cambia a 4G]                                             │
│    4G:   10.0.0.15:48920 → server:443     (Connection ID: X)│
│    → QUIC identifica la conexión por Connection ID, no IP.   │
│    → La conexión MIGRA sin interrupción.                     │
│    → Todas las peticiones en progreso continúan.             │
└──────────────────────────────────────────────────────────────┘
```

### QPACK (compresión de cabeceras en HTTP/3)

HTTP/3 no usa HPACK porque la tabla dinámica de HPACK requiere entrega en orden (un stream necesita saber qué añadieron streams anteriores a la tabla). QPACK resuelve esto permitiendo que los decodificadores funcionen con referencias a entradas que aún no han llegado:

```
HPACK (HTTP/2):
  Stream 1 añade "Cookie: abc" a tabla dinámica como índice 62
  Stream 3 referencia índice 62
  → Si stream 1 llega después de stream 3: ERROR

QPACK (HTTP/3):
  Usa un stream unidireccional dedicado para actualizar la tabla
  Los streams de datos pueden referenciar entradas pendientes
  → El orden de llegada no importa
```

---

## Comparativa práctica

```
┌────────────────────┬──────────────┬──────────────┬─────────────┐
│ Característica     │  HTTP/1.1    │   HTTP/2     │  HTTP/3     │
├────────────────────┼──────────────┼──────────────┼─────────────┤
│ Formato            │ Texto        │ Binario      │ Binario     │
│ Transporte         │ TCP          │ TCP          │ QUIC (UDP)  │
│ TLS                │ Opcional     │ De facto     │ Obligatorio │
│ Multiplexing       │ No           │ Sí           │ Sí          │
│ HOL blocking       │ HTTP + TCP   │ Solo TCP     │ Ninguno     │
│ Compresión headers │ No           │ HPACK        │ QPACK       │
│ Server push        │ No           │ Sí (en desuso)│ Sí (mínimo)│
│ Handshake (1ª vez) │ 2-3 RTTs     │ 2-3 RTTs     │ 1 RTT      │
│ Handshake (recon.) │ 1-2 RTTs     │ 1-2 RTTs     │ 0 RTT      │
│ Conn migration     │ No           │ No           │ Sí          │
│ Cancelar 1 request │ Cierra conn  │ RST_STREAM   │ RST_STREAM  │
│ RFC                │ 7230-7235    │ 9113         │ 9114        │
│ Año                │ 1997/1999    │ 2015         │ 2022        │
└────────────────────┴──────────────┴──────────────┴─────────────┘
```

### ¿Cuándo HTTP/3 marca la diferencia?

```
┌──────────────────────────────────────────────────────────────┐
│  HTTP/3 beneficia especialmente cuando:                      │
│                                                              │
│  • Alta latencia (redes móviles, conexiones intercont.)      │
│    → 0-RTT reduce significativamente el TTFB                │
│                                                              │
│  • Pérdida de paquetes frecuente (WiFi, 4G/5G)              │
│    → Sin TCP HOL blocking, los streams no afectados          │
│      continúan sin interrupción                              │
│                                                              │
│  • Cambio de red frecuente (móvil WiFi ↔ 4G)                │
│    → Connection migration mantiene las conexiones vivas      │
│                                                              │
│  HTTP/3 beneficia menos cuando:                              │
│                                                              │
│  • Red estable, baja latencia (datacenter, fibra)            │
│    → HTTP/2 ya es excelente en estas condiciones             │
│                                                              │
│  • Pocas peticiones simultáneas                              │
│    → Sin multiplexing intensivo, la diferencia es mínima     │
└──────────────────────────────────────────────────────────────┘
```

### Alt-Svc: cómo descubre el cliente que HTTP/3 está disponible

El servidor anuncia soporte HTTP/3 mediante la cabecera `Alt-Svc`:

```
# Primera petición va por HTTP/2 (TCP):
GET / HTTP/2
Host: example.com

HTTP/2 200 OK
Alt-Svc: h3=":443"; ma=86400
# "también estoy disponible en HTTP/3, puerto 443, válido 24h"

# Peticiones siguientes pueden usar HTTP/3 (QUIC):
GET / HTTP/3
...
```

El navegador mantiene ambas conexiones brevemente y migra a HTTP/3 si es más rápido (racing/Happy Eyeballs).

---

## Diagnóstico y herramientas

### curl

```bash
# Forzar HTTP/2
curl --http2 -v https://example.com

# Forzar HTTP/3 (requiere curl compilado con soporte QUIC)
curl --http3 -v https://example.com

# Ver qué protocolo se negoció
curl -sI https://example.com -o /dev/null -w "%{http_version}\n"
# Salida: "2" para HTTP/2, "3" para HTTP/3

# Ver si el servidor anuncia HTTP/3
curl -sI https://example.com | grep -i "alt-svc"
```

### nghttp (cliente HTTP/2 dedicado)

```bash
# Instalar
sudo dnf install nghttp2    # Fedora/RHEL
sudo apt install nghttp2     # Debian/Ubuntu

# Ver frames HTTP/2 detallados
nghttp -v https://example.com

# Salida muestra frames individuales:
# [  0.050] recv SETTINGS frame
# [  0.050] recv WINDOW_UPDATE frame
# [  0.050] send HEADERS frame (stream_id=1)
# [  0.100] recv HEADERS frame (stream_id=1)
# [  0.100] recv DATA frame (stream_id=1)
```

### Navegador (DevTools)

```
┌──────────────────────────────────────────────────────────────┐
│  Chrome/Firefox DevTools → Network tab                       │
│                                                              │
│  1. Columna "Protocol":                                      │
│     h2     = HTTP/2                                          │
│     h3     = HTTP/3                                          │
│     http/1.1 = HTTP/1.1                                      │
│                                                              │
│  2. Timing breakdown:                                        │
│     Connection Start → muestra si hubo nuevo handshake       │
│     Waiting (TTFB)   → tiempo hasta primer byte              │
│                                                              │
│  Si no ves la columna Protocol:                              │
│     Click derecho en cabecera de columnas → marcar Protocol  │
└──────────────────────────────────────────────────────────────┘
```

### Verificar soporte en el servidor

```bash
# ¿El servidor soporta HTTP/2?
curl -sI https://example.com -o /dev/null -w "%{http_version}\n"

# ¿Anuncia HTTP/3?
curl -sI https://example.com | grep -i alt-svc

# ¿QUIC está accesible? (necesita que el firewall permita UDP/443)
# Si Alt-Svc dice h3 pero HTTP/3 no funciona, probablemente
# un firewall bloquea UDP en el puerto 443
```

### Configuración nginx

```nginx
# HTTP/2 (automático con TLS en nginx ≥ 1.25.1)
server {
    listen 443 ssl;
    http2 on;
    ssl_certificate /etc/ssl/cert.pem;
    ssl_certificate_key /etc/ssl/key.pem;
}

# HTTP/3 (nginx ≥ 1.25.0 con módulo quic)
server {
    listen 443 ssl;
    listen 443 quic reuseport;    # QUIC sobre UDP
    http2 on;
    http3 on;

    ssl_certificate /etc/ssl/cert.pem;
    ssl_certificate_key /etc/ssl/key.pem;

    # Anunciar HTTP/3
    add_header Alt-Svc 'h3=":443"; ma=86400' always;
}
```

---

## Errores comunes

### 1. Seguir aplicando optimizaciones de HTTP/1.1 en HTTP/2

```
# INCORRECTO en HTTP/2:
# • Bundling (un solo app.bundle.js enorme)
#   → Invalida toda la caché por un cambio mínimo
#
# • Domain sharding (cdn1.example.com, cdn2.example.com)
#   → Impide multiplexing en una sola conexión
#   → Cada dominio requiere nuevo handshake TCP+TLS
#
# • Inlining CSS/JS/imágenes en el HTML
#   → No cacheable independientemente
#
# CORRECTO en HTTP/2:
# • Muchos archivos pequeños (granularidad de caché)
# • Todo en un solo dominio (máximo multiplexing)
# • Recursos separados (cada uno con su caché)
```

### 2. Asumir que HTTP/3 funciona sin abrir UDP en el firewall

```bash
# HTTP/2 usa TCP (puerto 443)
# HTTP/3 usa UDP (puerto 443)
#
# Un firewall que solo permite TCP/443 bloqueará HTTP/3.

# Verificar:
sudo firewall-cmd --list-ports    # ¿está 443/udp?
sudo ss -ulnp | grep 443         # ¿el servidor escucha UDP/443?

# Si falta:
sudo firewall-cmd --add-port=443/udp --permanent
sudo firewall-cmd --reload
```

### 3. Pensar que HTTP/3 siempre es más rápido

```
# En redes estables con baja latencia y sin pérdida de paquetes,
# HTTP/2 y HTTP/3 rinden casi igual.
#
# HTTP/3 brilla en:
# • Redes con pérdida de paquetes (móvil, WiFi congestionado)
# • Alta latencia (conexiones intercontinentales)
# • Cambios de red frecuentes (connection migration)
#
# En un datacenter con fibra y <1ms de latencia,
# la diferencia entre HTTP/2 y HTTP/3 es insignificante.
```

### 4. Confundir "QUIC usa UDP" con "QUIC no es fiable"

```
# UDP no tiene fiabilidad, pero QUIC SÍ la tiene.
# QUIC implementa retransmisión, control de flujo,
# control de congestión — todo lo que hace TCP.
#
# La diferencia: QUIC lo hace por STREAM, no por conexión.
# Esto elimina el TCP head-of-line blocking.
#
# Analogía:
#   TCP = un solo mostrador de correos (si una carta se pierde,
#         la cola se detiene hasta reenviarla)
#   QUIC = múltiples mostradores independientes (si una carta
#          se pierde en el mostrador 3, los demás siguen)
```

### 5. No verificar el protocolo negociado en producción

```bash
# Tu servidor puede estar configurado para HTTP/2
# pero servir HTTP/1.1 si ALPN falla:
curl -sI https://tu-sitio.com -o /dev/null -w "Protocol: HTTP/%{http_version}\n"

# Si dice HTTP/1.1 cuando esperas HTTP/2:
# • ¿El módulo HTTP/2 está habilitado?
# • ¿TLS está configurado con ALPN?
# • ¿Un proxy intermedio degrada el protocolo?
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│                HTTP/2 & HTTP/3 Cheatsheet                    │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  HTTP/2:                                                     │
│    Formato binario (frames de 9B header + payload)           │
│    1 conexión TCP, múltiples streams (multiplexing)          │
│    HPACK (compresión de cabeceras, tablas estática+dinámica) │
│    Server push (PUSH_PROMISE) — en desuso, preferir preload  │
│    Priorización → RFC 9218 Extensible Priorities             │
│    Negociación: ALPN durante TLS handshake                   │
│    Control de flujo: WINDOW_UPDATE por conexión y por stream │
│    RST_STREAM: cancelar 1 petición sin matar la conexión     │
│    Pseudo-headers: :method, :path, :scheme, :authority       │
│    Streams impares = cliente, pares = server push            │
│    Persiste: TCP HOL blocking (pérdida afecta TODOS)         │
│                                                              │
│  HTTP/3:                                                     │
│    QUIC sobre UDP (fiabilidad en espacio de usuario)         │
│    Elimina TCP HOL blocking (pérdida afecta solo 1 stream)   │
│    TLS 1.3 integrado en QUIC (1 RTT, 0-RTT en reconexión)   │
│    QPACK en vez de HPACK (funciona sin orden de entrega)     │
│    Connection migration (cambiar de red sin perder conexión) │
│    Descubrimiento: Alt-Svc header                            │
│    Requiere: firewall abierto en UDP/443                     │
│                                                              │
│  DIAGNÓSTICO:                                                │
│    curl -w "%{http_version}" URL        protocolo usado      │
│    curl --http2 -v URL                  forzar HTTP/2        │
│    curl --http3 -v URL                  forzar HTTP/3        │
│    curl -sI URL | grep alt-svc         ¿anuncia HTTP/3?     │
│    nghttp -v URL                        frames HTTP/2        │
│    DevTools → Network → Protocol        ver en navegador     │
│                                                              │
│  NGINX:                                                      │
│    http2 on;                            habilitar HTTP/2     │
│    listen 443 quic reuseport;           habilitar HTTP/3     │
│    http3 on;                                                 │
│    add_header Alt-Svc 'h3=":443"';     anunciar HTTP/3      │
│                                                              │
│  ANTI-PATRONES HTTP/1.1 (evitar en HTTP/2+):                 │
│    Domain sharding → 1 dominio (mejor multiplexing)          │
│    Bundling grande → archivos pequeños (mejor caché)         │
│    Inlining → recursos separados (cacheables)                │
│    Sprites → imágenes individuales                           │
│                                                              │
│  0-RTT:                                                      │
│    Solo para peticiones idempotentes (replay attack risk)     │
│    El servidor puede rechazar 0-RTT data                     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Identificar protocolos en producción

Usa `curl` para analizar tres sitios web diferentes:

```bash
# Para cada sitio, ejecuta:
curl -sI https://SITIO -o /dev/null -w "Protocol: HTTP/%{http_version}\n"
curl -sI https://SITIO | grep -i alt-svc
```

Sitios sugeridos: `https://www.google.com`, `https://www.cloudflare.com`, `https://example.com`

**Responde**:
1. ¿Qué protocolo negoció cada sitio?
2. ¿Alguno anuncia HTTP/3 vía `Alt-Svc`? ¿Qué dice exactamente la cabecera?
3. Si un sitio soporta HTTP/3, ¿por qué la primera petición de `curl` usó HTTP/2?
4. ¿Qué necesitarías para que `curl` use HTTP/3 directamente?

**Pregunta de reflexión**: Si un ISP bloquea UDP en el puerto 443, ¿qué protocolo usará el navegador como fallback? ¿El usuario notará alguna diferencia visible?

---

### Ejercicio 2: HTTP/1.1 vs HTTP/2 — impacto de multiplexing

Imagina una página web que necesita 30 recursos (1 HTML, 5 CSS, 10 JS, 14 imágenes). Cada recurso tarda 50ms en generarse y el RTT entre cliente y servidor es 100ms.

**Calcula** (aproximaciones simplificadas):

1. **HTTP/1.1 con 6 conexiones paralelas**:
   - ¿Cuántos RTTs se necesitan solo para los handshakes TCP+TLS de las 6 conexiones?
   - ¿Cuántas "rondas" de peticiones hacen falta para los 30 recursos (6 por ronda)?
   - Tiempo estimado total (handshakes + rondas × RTT).

2. **HTTP/2 con 1 conexión**:
   - ¿Cuántos RTTs para el único handshake TCP+TLS?
   - ¿Cuántas rondas de peticiones? (pista: multiplexing)
   - Tiempo estimado total.

3. **HTTP/3 con 0-RTT (reconexión)**:
   - ¿Cuántos RTTs para el handshake?
   - Tiempo estimado total.

**Pregunta de reflexión**: En tu cálculo de HTTP/2, ¿qué pasaría si se pierde un paquete TCP que contiene un frame del stream 5? ¿Cómo afecta esto a los otros 29 streams? ¿Y si fuera HTTP/3?

---

### Ejercicio 3: Diseñar una estrategia de migración

Eres sysadmin de un sitio web que actualmente usa HTTP/1.1 con nginx. El sitio tiene:
- Un frontend SPA servido como archivos estáticos.
- Una API REST backend en el puerto 3000.
- Usuarios en todo el mundo, muchos desde móviles.
- Firewall `firewalld` configurado.

Diseña un plan de migración paso a paso para llegar a HTTP/3:

1. ¿Qué cambios de configuración necesitas en nginx para habilitar HTTP/2?
2. ¿Qué optimizaciones de HTTP/1.1 deberías deshacer al migrar a HTTP/2?
3. ¿Qué requisitos adicionales hay para HTTP/3 (nginx, firewall, certificados)?
4. ¿Cómo verificarías que cada fase funciona correctamente?
5. ¿Qué cabecera debe añadir nginx para que los navegadores descubran HTTP/3?

Escribe los comandos y configuraciones concretos para cada paso.

**Pregunta de reflexión**: ¿Por qué es seguro habilitar HTTP/3 en producción sin deshabilitar HTTP/2? ¿Qué mecanismo garantiza que los clientes que no soportan HTTP/3 sigan funcionando?

---

> **Siguiente tema**: T05 — WebSocket (upgrade de HTTP, comunicación bidireccional, frames, cuándo usar sobre HTTP)
