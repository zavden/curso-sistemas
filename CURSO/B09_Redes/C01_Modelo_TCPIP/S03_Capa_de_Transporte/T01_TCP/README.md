# TCP: Three-Way Handshake, Estados, Ventana y Retransmisión

## Índice

1. [Qué es TCP](#1-qué-es-tcp)
2. [Three-way handshake](#2-three-way-handshake)
3. [Transferencia de datos](#3-transferencia-de-datos)
4. [Cierre de conexión](#4-cierre-de-conexión)
5. [Estados de conexión TCP](#5-estados-de-conexión-tcp)
6. [Diagrama completo de estados](#6-diagrama-completo-de-estados)
7. [Ventana deslizante y control de flujo](#7-ventana-deslizante-y-control-de-flujo)
8. [Control de congestión](#8-control-de-congestión)
9. [Retransmisión](#9-retransmisión)
10. [TCP en Linux](#10-tcp-en-linux)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Qué es TCP

TCP (Transmission Control Protocol) es el protocolo de transporte que proporciona
comunicación **fiable, ordenada y con control de errores** entre dos procesos.

```
Lo que TCP garantiza:
  ✔ Entrega de todos los datos (o notificación de fallo)
  ✔ Orden correcto (los datos llegan en la secuencia que se enviaron)
  ✔ Sin duplicados (cada byte se entrega exactamente una vez)
  ✔ Integridad (checksum detecta corrupción)
  ✔ Control de flujo (el emisor no satura al receptor)
  ✔ Control de congestión (el emisor no satura la red)

Lo que TCP NO garantiza:
  ✗ Latencia mínima (las retransmisiones añaden delay)
  ✗ Ancho de banda mínimo (la congestión lo reduce)
  ✗ Preservación de límites de mensaje (TCP es un stream de bytes)
```

### TCP es un stream, no mensajes

```
La aplicación escribe:   "Hello"  luego  "World"

TCP puede entregar:
  "HelloWorld"           ← una sola lectura
  "Hel" + "loWorld"      ← dos lecturas
  "H" + "e" + "lloWorld" ← tres lecturas

TCP garantiza el ORDEN de los bytes, no los límites de los mensajes.
La aplicación debe implementar su propio protocolo de framing
(ej: HTTP usa \r\n\r\n para separar headers del body).
```

---

## 2. Three-way handshake

### El proceso de establecimiento de conexión

Antes de transmitir datos, TCP establece una conexión con un intercambio de 3
segmentos:

```
Cliente                                  Servidor
  │                                        │
  │  1. SYN (seq=x)                        │
  │  "Quiero conectarme,                   │
  │   mi seq inicial es x"                 │
  │───────────────────────────────────────►│
  │                                        │
  │  2. SYN+ACK (seq=y, ack=x+1)          │
  │  "Acepto, mi seq inicial es y,         │
  │   espero tu byte x+1"                 │
  │◄───────────────────────────────────────│
  │                                        │
  │  3. ACK (seq=x+1, ack=y+1)            │
  │  "Confirmado, espero tu byte y+1"      │
  │───────────────────────────────────────►│
  │                                        │
  │  ═══ Conexión establecida ═══          │
  │  Ambos pueden enviar datos             │
```

### Por qué 3 pasos y no 2

```
Con 2 pasos:
  Cliente → SYN → Servidor
  Servidor → SYN+ACK → Cliente
  ¿Problema? El servidor asume que la conexión está lista,
  pero el cliente no ha confirmado que recibió el SYN+ACK.

  Si el SYN+ACK se pierde, el servidor tiene una conexión
  "abierta" que el cliente no sabe que existe.

  Peor: un SYN antiguo retrasado podría abrir conexiones fantasma.

Con 3 pasos:
  Ambos confirman que pueden enviar Y recibir.
  El servidor no reserva recursos hasta recibir el ACK final.
```

### Sequence numbers iniciales (ISN)

```
¿Por qué los seq numbers no empiezan en 0?

  Si siempre empezaran en 0, un segmento de una conexión anterior
  (retrasado en la red) podría confundirse con uno de la conexión actual.

  Los ISN se generan de forma pseudoaleatoria para evitar:
  1. Colisión con conexiones anteriores al mismo host:puerto
  2. Ataques de predicción de seq numbers (TCP hijacking)
```

### SYN flood (ataque)

```
Atacante envía miles de SYN sin completar el handshake:

  Atacante → SYN → Servidor     (servidor reserva memoria)
  Atacante → SYN → Servidor     (servidor reserva más memoria)
  Atacante → SYN → Servidor     (...)
  ... (nunca envía el ACK final)

  Resultado: el servidor agota su tabla de conexiones half-open
  → no puede aceptar conexiones legítimas

Mitigación: SYN cookies
  El servidor NO reserva estado con el SYN.
  Codifica la información en el seq number del SYN+ACK.
  Solo reserva estado si recibe el ACK final válido.

  Verificar en Linux:
  sysctl net.ipv4.tcp_syncookies
  # 1 = activado (default)
```

---

## 3. Transferencia de datos

### Sequence y Acknowledgment numbers

```
Después del handshake (seq cliente=1001, seq servidor=5001):

Cliente                                   Servidor
  │                                         │
  │  Datos "Hello" (5 bytes)                │
  │  seq=1001, len=5                        │
  │────────────────────────────────────────►│
  │                                         │
  │                     ACK ack=1006         │
  │                     "Recibí hasta 1005,  │
  │                      espero byte 1006"   │
  │◄────────────────────────────────────────│
  │                                         │
  │  Datos "World!" (6 bytes)               │
  │  seq=1006, len=6                        │
  │────────────────────────────────────────►│
  │                                         │
  │                     ACK ack=1012         │
  │◄────────────────────────────────────────│
```

- **seq**: posición del primer byte de datos en este segmento
- **ack**: el próximo byte que el receptor espera recibir
- **len**: longitud de datos en este segmento

### ACK acumulativo

TCP usa ACK acumulativo: el ack number confirma **todos** los bytes anteriores:

```
Segmento 1: seq=1000, len=100    (bytes 1000-1099)
Segmento 2: seq=1100, len=100    (bytes 1100-1199)
Segmento 3: seq=1200, len=100    (bytes 1200-1299)

Si los 3 llegan, el receptor envía: ACK ack=1300
  → "Recibí todo hasta 1299, espero 1300"
  → Un solo ACK confirma los 3 segmentos
```

### Delayed ACK

El receptor no envía un ACK por cada segmento individual. Espera brevemente (~40ms)
para:
1. Agrupar varios ACKs en uno solo
2. Enviar el ACK junto con datos de respuesta (piggybacking)

```
Sin delayed ACK:                 Con delayed ACK:
  Datos →                         Datos →
  ← ACK                          Datos →
  Datos →                         ← ACK (cubre ambos)
  ← ACK
  (4 segmentos)                   (3 segmentos, menos overhead)
```

---

## 4. Cierre de conexión

### Four-way termination (normal)

```
Cliente                                  Servidor
  │                                        │
  │  FIN (seq=x)                           │
  │  "No tengo más datos que enviar"       │
  │───────────────────────────────────────►│
  │                                        │
  │  ACK (ack=x+1)                         │
  │  "Recibido tu FIN"                     │
  │◄───────────────────────────────────────│
  │                                        │
  │  (servidor puede seguir enviando datos)│
  │                                        │
  │  FIN (seq=y)                           │
  │  "Yo tampoco tengo más datos"          │
  │◄───────────────────────────────────────│
  │                                        │
  │  ACK (ack=y+1)                         │
  │  "Recibido tu FIN"                     │
  │───────────────────────────────────────►│
  │                                        │
  │  ═══ Conexión cerrada ═══              │
```

### Half-close

TCP permite que un lado cierre su dirección de envío mientras el otro sigue enviando:

```
Cliente envía FIN → "Ya no enviaré más datos"
Pero el servidor puede seguir enviando datos hasta que él también envíe FIN.

Uso real: HTTP/1.0 — el cliente envía la petición y hace shutdown(SHUT_WR),
el servidor envía la respuesta y luego cierra.
```

### RST (Reset) — cierre abrupto

```
RST se envía cuando:
  - Conexión a un puerto que no tiene proceso escuchando
  - Aplicación decide abortar (en lugar de cerrar limpiamente)
  - Segmento llega para una conexión que no existe
  - Firewall interrumpe una conexión activa

RST no espera ACK. Cierra la conexión inmediatamente.
Los datos en tránsito se pierden.
```

```
Cliente                                  Servidor
  │                                        │
  │  RST                                   │
  │  "Abortar conexión inmediatamente"     │
  │───────────────────────────────────────►│
  │                                        │
  │  (no hay respuesta — conexión muerta)  │
```

---

## 5. Estados de conexión TCP

Cada conexión TCP pasa por una serie de estados. Entender estos estados es
fundamental para diagnosticar problemas con `ss` o `netstat`.

### Estados del cliente (conexión saliente)

```
CLOSED → SYN_SENT → ESTABLISHED → FIN_WAIT_1 → FIN_WAIT_2 → TIME_WAIT → CLOSED
```

### Estados del servidor (conexión entrante)

```
CLOSED → LISTEN → SYN_RECEIVED → ESTABLISHED → CLOSE_WAIT → LAST_ACK → CLOSED
```

### Tabla de estados

```
Estado          Quién          Qué significa
──────────────  ────────────── ──────────────────────────────────────
LISTEN          Servidor       Esperando conexiones entrantes
SYN_SENT        Cliente        SYN enviado, esperando SYN+ACK
SYN_RECEIVED    Servidor       SYN recibido, SYN+ACK enviado, esperando ACK
ESTABLISHED     Ambos          Conexión activa, datos fluyendo
FIN_WAIT_1      Lado que cierra FIN enviado, esperando ACK
FIN_WAIT_2      Lado que cierra ACK del FIN recibido, esperando FIN del otro
TIME_WAIT       Lado que cierra Ambos FIN intercambiados, esperando 2×MSL
CLOSE_WAIT      Lado pasivo     FIN recibido, ACK enviado, esperando que
                                la app cierre el socket
LAST_ACK        Lado pasivo     FIN enviado, esperando último ACK
CLOSING         Ambos           Ambos enviaron FIN simultáneamente (raro)
```

### Estados problemáticos

```
CLOSE_WAIT acumulándose:
  La aplicación recibió FIN del peer pero no cerró su socket.
  → BUG en la aplicación (no llama close() cuando debe)
  → Las conexiones se acumulan y consumen recursos

TIME_WAIT acumulándose:
  Normal en servidores con alto tráfico (muchas conexiones cortas).
  Cada conexión cerrada permanece 2×MSL (60 segundos típico).
  → No es un bug, pero puede agotar puertos efímeros.
  → Mitigación: tcp_tw_reuse
```

---

## 6. Diagrama completo de estados

```
                              ┌────────┐
                              │ CLOSED │
                              └───┬────┘
                    ┌─────────────┼─────────────┐
                    │ (cliente)   │  (servidor)  │
                    ▼             │              ▼
              ┌──────────┐       │        ┌──────────┐
              │ SYN_SENT │       │        │  LISTEN  │
              └────┬─────┘       │        └────┬─────┘
                   │             │             │
          rcv SYN+ACK            │         rcv SYN
          snd ACK                │         snd SYN+ACK
                   │             │             │
                   ▼             │             ▼
              ┌───────────────────────────┬──────────────┐
              │       ESTABLISHED         │ SYN_RECEIVED │
              │                           └──────┬───────┘
              │                                  │ rcv ACK
              │◄─────────────────────────────────┘
              │
              │  Cierre activo (envía FIN)        Cierre pasivo (recibe FIN)
              │                                          │
              ▼                                          ▼
        ┌────────────┐                            ┌────────────┐
        │ FIN_WAIT_1 │                            │ CLOSE_WAIT │
        └─────┬──────┘                            └─────┬──────┘
              │ rcv ACK                                 │ app cierra
              ▼                                         │ snd FIN
        ┌────────────┐                                  ▼
        │ FIN_WAIT_2 │                            ┌────────────┐
        └─────┬──────┘                            │  LAST_ACK  │
              │ rcv FIN                           └─────┬──────┘
              │ snd ACK                                 │ rcv ACK
              ▼                                         ▼
        ┌────────────┐                            ┌────────┐
        │ TIME_WAIT  │──── timeout 2×MSL ────────►│ CLOSED │
        └────────────┘                            └────────┘
```

---

## 7. Ventana deslizante y control de flujo

### El problema

Si el emisor envía datos más rápido de lo que el receptor puede procesarlos, el
buffer del receptor se llena y los datos se pierden.

### Window Size

El receptor anuncia una **ventana** (window size) en cada ACK: cuántos bytes puede
recibir antes de saturarse.

```
Receptor anuncia: Window = 4000 bytes

  El emisor puede enviar hasta 4000 bytes sin esperar ACK

  Segmento 1: 1000 bytes ──►  (quedan 3000 en la ventana)
  Segmento 2: 1000 bytes ──►  (quedan 2000)
  Segmento 3: 1000 bytes ──►  (quedan 1000)
  Segmento 4: 1000 bytes ──►  (ventana llena — esperar ACK)

  ◄── ACK + Window = 4000     (receptor procesó datos, ventana se reabre)

  Segmento 5: 1000 bytes ──►  (sigue enviando)
```

### Ventana deslizante (sliding window)

```
Bytes: 1  2  3  4  5  6  7  8  9  10  11  12  13  14  15
       ──────  ─────────────────  ──────────────────────
       ACKed   Enviados, sin ACK  Por enviar (en ventana)
       │       │                  │
       └───────┴──────────────────┘
               Ventana del emisor (window size)

Cuando llega un ACK, la ventana "se desliza" hacia la derecha:

       ACKed             Ventana deslizada →
       ──────────────    ─────────────────────
       1  2  3  4  5     6  7  8  9  10  11  12
```

### Window scaling

El campo Window en el header TCP tiene 16 bits → máximo 65,535 bytes. Para redes
modernas de alta velocidad esto es insuficiente. La opción **Window Scale** (negociada
en el handshake) permite multiplicar el valor:

```
Window = 65535, Scale factor = 7
Window efectiva = 65535 × 2^7 = 65535 × 128 = 8,388,480 bytes (~8 MB)
```

### Zero window

Si el receptor está saturado, anuncia Window = 0:

```
Emisor                                Receptor
  │                                      │
  │  Datos ──────────────────────────►   │ (buffer lleno)
  │                                      │
  │  ◄──── ACK, Window = 0              │ "¡Para! No puedo recibir más"
  │                                      │
  │  (emisor detiene envío)              │
  │                                      │
  │  Window Probe ───────────────────►   │ (emisor pregunta periódicamente)
  │                                      │
  │  ◄──── ACK, Window = 4000           │ "Ya puedo recibir, envía"
  │                                      │
  │  Datos ──────────────────────────►   │
```

---

## 8. Control de congestión

### Diferencia con control de flujo

```
Control de flujo:     Protege al RECEPTOR (no saturar su buffer)
                      Mecanismo: Window Size en el ACK

Control de congestión: Protege la RED (no saturar routers/enlaces)
                       Mecanismo: Congestion Window (cwnd) en el emisor
```

### Congestion Window (cwnd)

El emisor mantiene una ventana interna (`cwnd`) que limita cuánto puede enviar. La
ventana efectiva es el **mínimo** entre cwnd y la window del receptor:

```
Ventana efectiva = min(cwnd, receiver window)
```

### Algoritmos de control de congestión

```
1. SLOW START:
   cwnd empieza en 1 MSS
   Se duplica con cada ACK → crecimiento exponencial
   Hasta alcanzar ssthresh (slow start threshold)

   cwnd: 1 → 2 → 4 → 8 → 16 → ... (exponencial)

2. CONGESTION AVOIDANCE:
   Después de ssthresh, cwnd crece 1 MSS por RTT → crecimiento lineal

   cwnd: 16 → 17 → 18 → 19 → ... (lineal)

3. FAST RETRANSMIT:
   3 ACKs duplicados → asumir pérdida sin esperar timeout
   Retransmitir inmediatamente el segmento perdido

4. FAST RECOVERY:
   Tras fast retransmit, reducir cwnd a la mitad (no a 1)
   Continuar con congestion avoidance
```

```
cwnd
  ▲
  │                     ×  ← pérdida detectada
  │                   ╱    ssthresh = cwnd/2
  │                 ╱      cwnd = ssthresh
  │               ╱
  │             ╱  ← congestion avoidance (lineal)
  │           ╱
  │      ╱╱╱╱  ← slow start (exponencial)
  │    ╱╱
  │  ╱╱
  │╱╱
  └──────────────────────────────────► tiempo
```

### Algoritmos modernos en Linux

```bash
# Ver el algoritmo actual
sysctl net.ipv4.tcp_congestion_control
# cubic (default en Linux)

# Algoritmos disponibles
sysctl net.ipv4.tcp_available_congestion_control
# reno cubic bbr
```

| Algoritmo | Estrategia | Cuándo usar |
|---|---|---|
| **Reno** | Clásico, basado en pérdida | Legacy |
| **CUBIC** | Basado en pérdida, ventana cúbica | Default Linux, bueno para la mayoría |
| **BBR** | Basado en ancho de banda estimado | Redes con alta latencia o pérdida (Google lo usa) |

---

## 9. Retransmisión

### Retransmission Timeout (RTO)

Si el emisor no recibe ACK dentro del timeout, retransmite el segmento:

```
Emisor                              Receptor
  │                                     │
  │  Segmento seq=1000 ───────────────► │
  │                                     │ (se pierde)
  │                                     │
  │  ... espera RTO ...                 │
  │                                     │
  │  Retransmisión seq=1000 ──────────► │
  │                                     │
  │  ◄──────────── ACK ack=1100        │
```

### Cómo se calcula el RTO

```
RTO se basa en mediciones del RTT (Round Trip Time):

  SRTT = (1-α) × SRTT + α × RTT_medido    (smoothed RTT, α=1/8)
  RTTVAR = (1-β) × RTTVAR + β × |SRTT - RTT_medido|  (varianza, β=1/4)
  RTO = SRTT + 4 × RTTVAR

  RTO mínimo: 200ms (Linux)
  RTO máximo: 120 segundos

Si se pierde un paquete y el retransmitido también:
  RTO se duplica (exponential backoff):
  200ms → 400ms → 800ms → 1.6s → 3.2s → ... → 120s
```

### Fast retransmit

En lugar de esperar el timeout completo, 3 ACKs duplicados indican pérdida:

```
Emisor                              Receptor
  │  seq=1000, len=100 ──────────►     │  ACK 1100
  │  seq=1100, len=100 ──────────►     │  (se pierde)
  │  seq=1200, len=100 ──────────►     │  ACK 1100 (duplicado 1)
  │  seq=1300, len=100 ──────────►     │  ACK 1100 (duplicado 2)
  │  seq=1400, len=100 ──────────►     │  ACK 1100 (duplicado 3)
  │                                     │
  │  ¡3 dup ACKs! Retransmitir 1100     │
  │  seq=1100, len=100 ──────────►     │  ACK 1500 (todos recibidos)
```

El receptor envía ACK 1100 repetidamente porque recibió bytes 1200+
pero le falta el 1100. Los ACKs son acumulativos — no puede confirmar
1200 sin haber recibido 1100.

### Selective ACK (SACK)

Sin SACK, ante una pérdida el emisor podría retransmitir segmentos que el receptor
ya tiene. SACK permite al receptor decir exactamente qué rangos ya recibió:

```
Sin SACK:
  "Recibí hasta 1100" → emisor retransmite 1100, 1200, 1300, 1400, 1500...
  (aunque el receptor ya tenía 1200-1500)

Con SACK:
  "Recibí hasta 1100, SACK 1200-1500" → emisor solo retransmite 1100
  (ahorra ancho de banda)
```

```bash
# SACK está habilitado por defecto en Linux
sysctl net.ipv4.tcp_sack
# 1
```

---

## 10. TCP en Linux

### Ver conexiones TCP

```bash
# Todas las conexiones TCP
ss -tn

# Solo las que escuchan (LISTEN)
ss -tlnp

# Con información de timer (retransmisiones, keepalive)
ss -tnoi

# Filtrar por estado
ss -tn state established
ss -tn state time-wait
ss -tn state close-wait
```

Salida de `ss -tnp`:

```
State    Recv-Q  Send-Q  Local Address:Port   Peer Address:Port   Process
ESTAB    0       0       192.168.1.10:22      192.168.1.5:54321   sshd
ESTAB    0       36      192.168.1.10:80      10.0.0.5:49876      nginx
```

- **Recv-Q**: bytes en el buffer de recepción no leídos por la app
- **Send-Q**: bytes enviados pero no ACKed por el peer

`Recv-Q` alto = la aplicación no lee datos suficientemente rápido.
`Send-Q` alto = la red o el peer son lentos (congestión o ventana llena).

### Parámetros TCP del kernel

```bash
# Tamaño de buffers TCP (auto-tuning)
sysctl net.ipv4.tcp_rmem    # Buffer de lectura: min default max
# 4096  131072  6291456     (4K  128K  6M)

sysctl net.ipv4.tcp_wmem    # Buffer de escritura: min default max
# 4096  16384   4194304     (4K  16K   4M)

# Keepalive (detectar conexiones muertas)
sysctl net.ipv4.tcp_keepalive_time      # 7200 (segundos hasta primer probe)
sysctl net.ipv4.tcp_keepalive_intvl     # 75   (segundos entre probes)
sysctl net.ipv4.tcp_keepalive_probes    # 9    (probes antes de declarar muerta)

# TIME_WAIT
sysctl net.ipv4.tcp_tw_reuse           # 0/1 — reutilizar sockets en TIME_WAIT
sysctl net.ipv4.tcp_fin_timeout        # 60  — timeout para FIN_WAIT_2

# SYN cookies
sysctl net.ipv4.tcp_syncookies         # 1 = activado (protección SYN flood)

# Retransmisiones máximas
sysctl net.ipv4.tcp_retries2           # 15 (intentos antes de abortar)
```

### TIME_WAIT y reutilización de puertos

```bash
# Contar conexiones en TIME_WAIT
ss -tn state time-wait | wc -l

# Si hay miles (servidor con alto tráfico):
# Opción 1: habilitar tcp_tw_reuse (seguro para conexiones salientes)
sudo sysctl -w net.ipv4.tcp_tw_reuse=1

# Opción 2: SO_REUSEADDR en la aplicación (para bind() al mismo puerto)
# Esto es responsabilidad del código de la aplicación, no del kernel
```

### Capturar handshake con tcpdump

```bash
# Ver el three-way handshake
sudo tcpdump -i enp0s3 -n "tcp[tcpflags] & (tcp-syn|tcp-fin|tcp-rst) != 0" -c 10
```

---

## 11. Errores comunes

### Error 1: Confundir FIN con RST

```
FIN:  "He terminado de enviar datos, cerremos limpiamente"
      → El otro lado puede seguir enviando (half-close)
      → Los datos en tránsito se entregan

RST:  "Abortar la conexión AHORA"
      → Todo se descarta inmediatamente
      → Datos en tránsito se pierden
      → No hay handshake de cierre

En logs, conexiones cerradas con RST indican problemas:
  - Crash de la aplicación
  - Firewall cortando conexiones
  - Conexión a un puerto cerrado
```

### Error 2: CLOSE_WAIT acumulándose

```bash
ss -tn state close-wait | wc -l
# 500  ← problema

CLOSE_WAIT = el peer envió FIN pero tu aplicación no llamó close().
Es SIEMPRE un bug en la aplicación local (no del peer, no de la red).

Solución: revisar el código de la aplicación — buscar sockets
que se abren pero no se cierran al terminar.
```

### Error 3: Pensar que TIME_WAIT es un problema

```
TIME_WAIT es NORMAL y NECESARIO:
  - Asegura que el último ACK llegue al peer
  - Evita que segmentos retrasados de la conexión anterior
    se confundan con la conexión nueva

TIME_WAIT es un PROBLEMA solo cuando:
  - Hay tantas que agotan los puertos efímeros (~30,000)
  - Un servidor web de alto tráfico con conexiones cortas

Solución: tcp_tw_reuse, no tcp_tw_recycle (deprecated y peligroso)
```

### Error 4: Ignorar Recv-Q y Send-Q

```bash
ss -tnp
# ESTAB  45000  0  ...   ← Recv-Q = 45000
#   La aplicación no lee del socket → se acumula

# ESTAB  0  128000  ...  ← Send-Q = 128000
#   La red/peer no puede absorber datos → backpressure
```

Estas métricas son la primera pista cuando "la conexión es lenta".

### Error 5: No entender que TCP es un stream

```python
# BUG CLÁSICO: asumir que cada send() = un recv() separado
# Emisor:
socket.send(b"msg1")    # 4 bytes
socket.send(b"msg2")    # 4 bytes

# Receptor:
data = socket.recv(1024)
# data podría ser: b"msg1msg2" (ambos juntos)
#                  b"msg1"     (solo el primero)
#                  b"msg"      (parcial)

# La aplicación DEBE implementar framing:
# - Delimitador (ej: \n)
# - Longitud prefijada (ej: 4 bytes de longitud + datos)
```

---

## 12. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║                        TCP — REFERENCIA RÁPIDA                       ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                      ║
║  THREE-WAY HANDSHAKE                                                 ║
║  SYN → SYN+ACK → ACK      (3 pasos para establecer conexión)        ║
║                                                                      ║
║  CIERRE                                                              ║
║  FIN → ACK → FIN → ACK    (4 pasos, cierre limpio)                  ║
║  RST                       (cierre abrupto, sin handshake)           ║
║                                                                      ║
║  ESTADOS CLAVE                                                       ║
║  LISTEN        Servidor esperando conexiones                         ║
║  ESTABLISHED   Conexión activa                                       ║
║  TIME_WAIT     Cierre completado, esperando 2×MSL (normal)           ║
║  CLOSE_WAIT    Peer cerró, app local no ha cerrado (bug si persiste) ║
║  FIN_WAIT_2    FIN enviado y ACKed, esperando FIN del peer           ║
║                                                                      ║
║  CONTROL DE FLUJO                                                    ║
║  Window Size en ACK → cuántos bytes puede recibir el receptor        ║
║  Zero Window → "para de enviar, estoy saturado"                      ║
║  Window Scaling → permite ventanas > 65KB (negociado en handshake)   ║
║                                                                      ║
║  CONTROL DE CONGESTIÓN                                               ║
║  Slow Start → exponencial hasta ssthresh                             ║
║  Congestion Avoidance → lineal después de ssthresh                   ║
║  Fast Retransmit → 3 dup ACKs = retransmitir sin esperar timeout     ║
║                                                                      ║
║  RETRANSMISIÓN                                                       ║
║  RTO basado en SRTT + 4×RTTVAR                                       ║
║  Exponential backoff: RTO se duplica en cada intento                 ║
║  SACK: receptor informa qué rangos ya tiene → menos retransmisiones  ║
║                                                                      ║
║  COMANDOS LINUX                                                      ║
║  ss -tn                      Conexiones TCP activas                  ║
║  ss -tlnp                    Puertos escuchando                      ║
║  ss -tn state time-wait      Filtrar por estado                      ║
║  ss -tnoi                    Con info de timers                      ║
║  sysctl net.ipv4.tcp_*       Parámetros TCP del kernel               ║
║  tcpdump "tcp[tcpflags]..."  Capturar flags específicos              ║
║                                                                      ║
║  DIAGNÓSTICO RÁPIDO                                                  ║
║  Recv-Q alto → app no lee del socket                                 ║
║  Send-Q alto → red/peer lentos                                       ║
║  CLOSE_WAIT acumulado → bug en la app (no cierra sockets)            ║
║  TIME_WAIT excesivo → servidor de alto tráfico (tcp_tw_reuse)        ║
║  RST frecuentes → firewall, crash, o puerto cerrado                 ║
║                                                                      ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 13. Ejercicios

### Ejercicio 1: Observar el three-way handshake

1. En una terminal, inicia una captura de handshakes:
   ```bash
   sudo tcpdump -i enp0s3 -n "tcp[tcpflags] & (tcp-syn|tcp-fin|tcp-rst) != 0" -c 6
   ```
2. En otra terminal, genera una conexión TCP:
   ```bash
   curl -s http://example.com > /dev/null
   ```
3. En la captura de tcpdump, identifica:
   - El SYN (flags [S])
   - El SYN+ACK (flags [S.])
   - El ACK (flags [.])
   - Los seq numbers iniciales de cada lado
   - El FIN o RST de cierre

**Pregunta de predicción**: ¿cuántos paquetes con flags de control esperas ver para
una petición HTTP completa? (handshake + cierre)

**Pregunta de reflexión**: si ves un RST en vez de FIN+ACK al cerrar, ¿qué indica
sobre cómo el servidor maneja la conexión?

### Ejercicio 2: Estados TCP en tu sistema

1. Lista todas las conexiones TCP por estado:
   ```bash
   ss -tan | awk '{print $1}' | sort | uniq -c | sort -rn
   ```
2. Identifica:
   - ¿Cuántas conexiones en ESTABLISHED?
   - ¿Hay alguna en CLOSE_WAIT? (potencial problema)
   - ¿Cuántas en TIME_WAIT?
   - ¿Qué servicios están en LISTEN?

3. Observa los buffers:
   ```bash
   ss -tn | awk '$2 > 0 || $3 > 0'
   ```
   ¿Hay alguna conexión con Recv-Q o Send-Q mayor que 0?

4. Revisa los parámetros TCP del kernel:
   ```bash
   sysctl net.ipv4.tcp_syncookies
   sysctl net.ipv4.tcp_tw_reuse
   sysctl net.ipv4.tcp_congestion_control
   sysctl net.ipv4.tcp_sack
   ```

**Pregunta de reflexión**: si ves 200 conexiones en CLOSE_WAIT todas del mismo proceso,
¿es un problema de red o de la aplicación? ¿Cómo lo resolverías?

### Ejercicio 3: Simular control de flujo

1. Inicia un servidor TCP lento que lee datos despacio:
   ```bash
   # Servidor que lee 1 byte cada segundo
   python3 -c "
   import socket, time
   s = socket.socket()
   s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
   s.bind(('0.0.0.0', 9999))
   s.listen(1)
   conn, addr = s.accept()
   print(f'Connected: {addr}')
   while True:
       data = conn.recv(1)
       if not data: break
       time.sleep(1)
   "
   ```

2. En otra terminal, envía datos rápidamente:
   ```bash
   dd if=/dev/zero bs=1M count=10 | nc localhost 9999
   ```

3. En una tercera terminal, observa el Send-Q crecer:
   ```bash
   watch -n 0.5 "ss -tn 'sport = :9999 or dport = :9999'"
   ```

**Pregunta de predicción**: ¿qué pasará con el Send-Q del cliente? ¿Crecerá
indefinidamente o se estabilizará? ¿Por qué?

**Pregunta de reflexión**: el mecanismo de Window Size que observas aquí es control
de flujo (protege al receptor). ¿Cómo se diferencia del control de congestión que
protege a la red? ¿Pueden actuar ambos simultáneamente?
