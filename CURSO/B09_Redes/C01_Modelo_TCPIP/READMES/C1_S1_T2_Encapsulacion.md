# Encapsulación: Headers, Payload, MTU y Fragmentación

## Índice

1. [Qué es la encapsulación](#1-qué-es-la-encapsulación)
2. [El proceso paso a paso](#2-el-proceso-paso-a-paso)
3. [Anatomía de cada capa](#3-anatomía-de-cada-capa)
4. [Desencapsulación](#4-desencapsulación)
5. [MTU (Maximum Transmission Unit)](#5-mtu-maximum-transmission-unit)
6. [Fragmentación IP](#6-fragmentación-ip)
7. [Path MTU Discovery](#7-path-mtu-discovery)
8. [MSS (Maximum Segment Size)](#8-mss-maximum-segment-size)
9. [Ver encapsulación en la práctica](#9-ver-encapsulación-en-la-práctica)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué es la encapsulación

La encapsulación es el proceso por el cual cada capa del modelo TCP/IP **envuelve**
los datos de la capa superior añadiéndoles su propio header (y a veces un trailer).
Es como poner una carta dentro de un sobre, y ese sobre dentro de otro sobre más
grande.

```
Datos de la aplicación:   "GET /index.html HTTP/1.1..."

Encapsulación:

  Aplicación genera:   [      Datos HTTP       ]

  Transporte añade:    [ TCP ][ Datos HTTP      ]

  Red añade:           [ IP  ][ TCP ][ Datos HTTP ]

  Enlace añade:        [ ETH ][ IP ][ TCP ][ Datos HTTP ][ FCS ]
                       header                             trailer
```

Cada capa trata todo lo que recibe de la capa superior como **payload** (carga útil)
opaco — no le importa qué contiene, solo lo envuelve con su header y lo pasa hacia
abajo.

### Terminología

```
┌──────────────────────────────────────────────────┐
│  HEADER   │           PAYLOAD                     │  TRAILER
│  (cabecera)│          (carga útil)                │  (opcional)
└──────────────────────────────────────────────────┘
             │                                      │
             └── El payload de una capa es el ──────┘
                 header + payload de la capa superior
```

| Término | Significado |
|---|---|
| **Header** | Metadatos que la capa añade al inicio (direcciones, flags, longitud) |
| **Payload** | Los datos que la capa transporta (todo lo de la capa superior) |
| **Trailer** | Metadatos al final (solo Ethernet usa trailer: el FCS/CRC para verificación) |
| **PDU** | Protocol Data Unit — nombre genérico de la unidad de datos en cada capa |

---

## 2. El proceso paso a paso

### Emisor: encapsulación (de arriba hacia abajo)

```
Paso 1: La aplicación genera datos
┌─────────────────────────────────────┐
│  "GET /index.html HTTP/1.1\r\n..."  │
└─────────────────────────────────────┘
                  │
                  ▼
Paso 2: TCP añade su header → SEGMENTO
┌──────┬─────────────────────────────────────┐
│ TCP  │  "GET /index.html HTTP/1.1\r\n..."  │
│ hdr  │          (payload TCP)               │
│20-60B│                                      │
└──────┴─────────────────────────────────────┘
                  │
                  ▼
Paso 3: IP añade su header → PAQUETE
┌──────┬──────┬─────────────────────────────────────┐
│  IP  │ TCP  │  "GET /index.html HTTP/1.1\r\n..."  │
│ hdr  │ hdr  │          (payload IP)                │
│ 20B  │      │                                      │
└──────┴──────┴─────────────────────────────────────┘
                  │
                  ▼
Paso 4: Ethernet añade header y trailer → FRAME
┌──────┬──────┬──────┬──────────────────────────┬─────┐
│ ETH  │  IP  │ TCP  │  Datos HTTP              │ FCS │
│ hdr  │ hdr  │ hdr  │                          │ 4B  │
│ 14B  │ 20B  │ 20B  │                          │     │
└──────┴──────┴──────┴──────────────────────────┴─────┘
                  │
                  ▼
         Señales eléctricas/ópticas en el cable
```

### Nombres de la PDU en cada capa

```
Capa             PDU              Contiene
──────────────── ──────────────── ──────────────────────────
Aplicación       Datos/Mensaje    Contenido puro de la app
Transporte       Segmento (TCP)   TCP header + datos app
                 Datagrama (UDP)  UDP header + datos app
Red              Paquete          IP header + segmento/datagrama
Enlace           Frame            ETH header + paquete + FCS
```

---

## 3. Anatomía de cada capa

### Header Ethernet (14 bytes)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
├───────────────────────────────────────────────────────────────────┤
│                    Destination MAC (6 bytes)                       │
├───────────────────────────────────────────────────────────────────┤
│                    Source MAC (6 bytes)                            │
├───────────────────────────────────────────────────────────────────┤
│         EtherType (2 bytes)                                       │
│         0x0800 = IPv4                                             │
│         0x0806 = ARP                                              │
│         0x86DD = IPv6                                             │
└───────────────────────────────────────────────────────────────────┘
Trailer: FCS (Frame Check Sequence) — 4 bytes CRC32 al final del frame
```

### Header IPv4 (20 bytes mínimo)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
├─┬─┬───────────┬───────────────┬───────────────────────────────────┤
│V│L│   ToS     │          Total Length                             │
│4│5│           │          (header + payload en bytes)              │
├─┴─┴───────────┼──┬────────────┴───────────────────────────────────┤
│ Identification │Fl│     Fragment Offset                            │
│                │ag│     (para fragmentación)                       │
├────────────────┴──┴───────────────────────────────────────────────┤
│   TTL (8b)    │ Protocol (8b) │     Header Checksum               │
│  (saltos max) │ 6=TCP,17=UDP  │                                   │
├───────────────┴───────────────┴───────────────────────────────────┤
│                    Source IP Address (32 bits)                     │
├───────────────────────────────────────────────────────────────────┤
│                 Destination IP Address (32 bits)                   │
└───────────────────────────────────────────────────────────────────┘
```

Campos clave:
- **TTL**: se decrementa en cada router. Si llega a 0, el paquete se descarta
- **Protocol**: indica qué protocolo hay en el payload (6=TCP, 17=UDP, 1=ICMP)
- **Total Length**: tamaño total del paquete IP (header + payload), máximo 65535 bytes
- **Fragment Offset + Flags**: usados para fragmentación (explicado más adelante)

### Header TCP (20 bytes mínimo)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
├───────────────────────────────┬───────────────────────────────────┤
│     Source Port (16 bits)     │   Destination Port (16 bits)      │
├───────────────────────────────┴───────────────────────────────────┤
│                    Sequence Number (32 bits)                       │
├───────────────────────────────────────────────────────────────────┤
│                 Acknowledgment Number (32 bits)                    │
├────────┬──────┬─┬─┬─┬─┬─┬─┬──────────────────────────────────────┤
│Data Off│Reserv│U│A│P│R│S│F│         Window Size                   │
│ (4b)   │      │R│C│S│S│Y│I│                                       │
│        │      │G│K│H│T│N│N│                                       │
├────────┴──────┴─┴─┴─┴─┴─┴─┴──────────────────────────────────────┤
│        Checksum              │      Urgent Pointer                │
└──────────────────────────────┴────────────────────────────────────┘
```

Campos clave:
- **Source/Destination Port**: identifican los procesos en cada extremo
- **Sequence Number**: posición del primer byte de datos en el flujo
- **Acknowledgment Number**: próximo byte que el receptor espera
- **Flags**: SYN (iniciar), ACK (confirmar), FIN (cerrar), RST (abortar)
- **Window Size**: control de flujo — cuántos bytes puede recibir sin saturarse

### Header UDP (8 bytes — mucho más simple)

```
├───────────────────────────────┬───────────────────────────────────┤
│     Source Port (16 bits)     │   Destination Port (16 bits)      │
├───────────────────────────────┼───────────────────────────────────┤
│     Length (16 bits)          │   Checksum (16 bits)              │
└───────────────────────────────┴───────────────────────────────────┘
```

UDP no tiene sequence numbers, acknowledgments, ni flags — es deliberadamente simple.

---

## 4. Desencapsulación

El receptor hace el proceso inverso: quita headers de abajo hacia arriba.

```
Frame llega por el cable
┌──────┬──────┬──────┬──────────────────────────┬─────┐
│ ETH  │  IP  │ TCP  │  Datos HTTP              │ FCS │
└──┬───┴──────┴──────┴──────────────────────────┴──┬──┘
   │                                               │
   ▼                                               ▼
Capa de enlace:
  - Verifica FCS (CRC) → ¿frame corrupto?
  - Lee EtherType → 0x0800 → es IPv4
  - Quita header Ethernet y FCS
  - Pasa el payload a la capa de red

┌──────┬──────┬──────────────────────────┐
│  IP  │ TCP  │  Datos HTTP              │
└──┬───┴──────┴──────────────────────────┘
   │
   ▼
Capa de red:
  - Verifica IP destino → ¿es para mí?
  - Lee Protocol → 6 → es TCP
  - Quita header IP
  - Pasa el payload a la capa de transporte

┌──────┬──────────────────────────┐
│ TCP  │  Datos HTTP              │
└──┬───┴──────────────────────────┘
   │
   ▼
Capa de transporte:
  - Lee puerto destino → 80 → nginx
  - Verifica sequence number, ACK
  - Quita header TCP
  - Pasa los datos a la aplicación

┌──────────────────────────┐
│  "GET /index.html..."    │  → nginx procesa la petición
└──────────────────────────┘
```

### Cada capa toma decisiones basándose en su header

```
Enlace:     MAC destino → ¿es mi MAC? → sí → procesar
Red:        IP destino  → ¿es mi IP?  → sí → procesar
                                       → no → soy router? → reenviar
Transporte: Puerto dest → ¿qué proceso escucha? → entregar
```

---

## 5. MTU (Maximum Transmission Unit)

### Qué es

El MTU es el tamaño máximo del **payload** que puede llevar un frame en la capa de
enlace. No incluye el header ni trailer del frame.

```
Frame Ethernet:
┌──────────┬──────────────────────────────────────────┬─────┐
│ ETH hdr  │              PAYLOAD                     │ FCS │
│  14 B    │          máx 1500 bytes (MTU)            │ 4 B │
└──────────┴──────────────────────────────────────────┴─────┘
                     ▲
                     │
              Esto es el MTU = 1500 bytes
              (Ethernet estándar)
```

### Implicaciones para el tamaño de datos

```
MTU Ethernet:         1500 bytes
- IP header:          - 20 bytes
- TCP header:         - 20 bytes
                      ──────────
Datos de aplicación:   1460 bytes máximo por segmento TCP

Si quieres enviar 5000 bytes de datos HTTP:
  5000 / 1460 = 3.42 → necesitas al menos 4 segmentos TCP
```

### Ver y cambiar el MTU

```bash
# Ver MTU actual
ip link show enp0s3
# ... mtu 1500 ...

# Cambiar MTU temporalmente
sudo ip link set enp0s3 mtu 9000

# Cambiar MTU permanentemente (NetworkManager)
sudo nmcli connection modify "Wired" 802-3-ethernet.mtu 9000
```

### Jumbo frames

Algunas redes LAN soportan MTU de **9000 bytes** (jumbo frames). Ventaja: menos
overhead de headers para transferencias grandes. Requisito: **todos** los dispositivos
en la ruta deben soportarlo.

```
MTU estándar (1500):
  Header overhead por cada 1500 bytes:  14 + 20 + 20 = 54 bytes → 3.6%

Jumbo frame (9000):
  Header overhead por cada 9000 bytes:  14 + 20 + 20 = 54 bytes → 0.6%
```

---

## 6. Fragmentación IP

### Cuándo ocurre

Si un paquete IP es más grande que el MTU del siguiente enlace, el router debe
**fragmentarlo** en paquetes más pequeños. Cada fragmento viaja independientemente
y se reensambla en el destino final.

```
Paquete original: 4000 bytes de payload IP
MTU del enlace: 1500

                    Router
Paquete             fragmenta              Destino
original  ────►    ┌─────────┐  ────►     reensambla
[4000 B]           │Frag 1   │            [4000 B]
                   │[1480 B] │
                   ├─────────┤
                   │Frag 2   │
                   │[1480 B] │
                   ├─────────┤
                   │Frag 3   │
                   │[1040 B] │
                   └─────────┘

Cada fragmento:
  - Tiene su propio header IP (20 bytes)
  - Comparte el mismo Identification number
  - Tiene un Fragment Offset que indica su posición
  - El último fragmento tiene el flag MF (More Fragments) = 0
```

### Campos IP involucrados

```
┌────────────────┬─────────────────────────────────────────────┐
│ Campo          │ Función                                      │
├────────────────┼─────────────────────────────────────────────┤
│ Identification │ Mismo valor en todos los fragmentos de un    │
│                │ paquete original → permite reensamblaje       │
├────────────────┼─────────────────────────────────────────────┤
│ Flags          │ DF (Don't Fragment): si =1, no fragmentar   │
│                │ MF (More Fragments): si =1, vienen más       │
├────────────────┼─────────────────────────────────────────────┤
│ Fragment Offset│ Posición del fragmento en unidades de 8 bytes│
│                │ Fragmento 1: offset=0                        │
│                │ Fragmento 2: offset=185 (1480/8)             │
│                │ Fragmento 3: offset=370 (2960/8)             │
└────────────────┴─────────────────────────────────────────────┘
```

### Por qué la fragmentación es mala

```
Problemas de la fragmentación:

1. Pérdida amplificada:
   Si se pierde 1 fragmento de 3, el destino descarta los 3
   y todo el paquete original se retransmite

2. Overhead de headers:
   Cada fragmento lleva su propio header IP (20 bytes extra)

3. Carga en el destino:
   El destino debe mantener buffer para fragmentos parciales
   esperando a que lleguen los demás

4. Firewall/NAT:
   Solo el primer fragmento tiene el header TCP/UDP con puertos
   Los demás fragmentos no tienen puertos → difícil filtrar
```

Por esto, la mayoría de sistemas modernos **evitan la fragmentación** usando Path MTU
Discovery.

---

## 7. Path MTU Discovery

### Cómo funciona

En lugar de dejar que los routers fragmenten, el emisor **descubre el MTU más pequeño
en toda la ruta** y ajusta el tamaño de sus paquetes antes de enviarlos.

```
Tu PC              Router A           Router B           Servidor
MTU=1500           MTU=1500           MTU=1400           MTU=1500
   │                  │                  │                  │
   │  Paquete 1500B   │                  │                  │
   │  DF=1 (no frag)  │                  │                  │
   │─────────────────►│                  │                  │
   │                  │  Paquete 1500B   │                  │
   │                  │  DF=1            │                  │
   │                  │─────────────────►│                  │
   │                  │                  │                  │
   │                  │  ICMP: "Packet   │                  │
   │  ICMP: "Packet   │  Too Big,        │                  │
   │  Too Big,         │  MTU=1400"       │                  │
   │  MTU=1400"        │◄────────────────│                  │
   │◄─────────────────│                  │                  │
   │                                                        │
   │  Paquete 1400B (ajustado)                              │
   │  DF=1                                                  │
   │───────────────────────────────────────────────────────►│
   │                                                        │
```

### El flag DF (Don't Fragment)

```
DF = 1: "No me fragmentes. Si soy muy grande, devuelve un error ICMP."
DF = 0: "Puedes fragmentarme si es necesario."

Linux por defecto envía paquetes TCP con DF=1
→ fuerza Path MTU Discovery
→ evita fragmentación
```

### Verificar el Path MTU

```bash
# Descubrir el MTU hacia un destino
tracepath example.com
#  1:  _gateway                              0.432ms pmtu 1500
#  2:  10.0.0.1                              2.123ms pmtu 1400
#  3:  example.com                           15.234ms reached
#      Resume: pmtu 1400

# Probar manualmente con ping
# -M do = set DF flag (don't fragment)
# -s 1472 = payload size (1472 + 8 ICMP + 20 IP = 1500)
ping -M do -s 1472 -c 1 example.com
# Si funciona: MTU >= 1500
# Si falla: "message too long" → MTU < 1500, reducir -s
```

### Problemas comunes con Path MTU Discovery

Si un firewall intermedio **bloquea ICMP**, el mensaje "Packet Too Big" nunca llega
al emisor. El resultado: paquetes grandes se pierden silenciosamente. Esto se llama
**PMTU black hole**.

```
Tu PC ──── Firewall (bloquea ICMP) ──── Router (MTU bajo) ──── Servidor

Paquetes pequeños:  ✔ pasan sin problemas
Paquetes grandes:   ✗ Router descarta, envía ICMP, firewall bloquea ICMP
                      → tu PC nunca se entera → conexión se "cuelga"

Síntoma típico: ssh conecta pero scp se cuelga al transferir archivos
                curl descarga headers pero el body nunca llega
```

---

## 8. MSS (Maximum Segment Size)

### Relación entre MTU y MSS

MSS es un concepto de la **capa de transporte** (TCP). Indica el tamaño máximo de
datos de aplicación que TCP puede enviar en un solo segmento.

```
MTU = MSS + IP header + TCP header

Para Ethernet estándar:
  1500 = MSS + 20 + 20
  MSS = 1460 bytes

Para un enlace con MTU 1400:
  1400 = MSS + 20 + 20
  MSS = 1360 bytes
```

### Cómo se negocia

TCP anuncia su MSS durante el three-way handshake usando una opción en el header TCP:

```
Cliente → Servidor:  SYN, MSS=1460
Servidor → Cliente:  SYN+ACK, MSS=1360
                     (el servidor tiene un MTU menor)

Resultado: ambos usan MSS=1360 (el menor de los dos)
→ los segmentos nunca excederán el MTU de ninguno de los dos extremos
```

### MSS vs MTU — diferencia conceptual

```
MTU:  ┌──────────────────────────────────────────┐
      │ ETH │ IP │ TCP │     DATOS              │ FCS │
      └──────────────────────────────────────────┘
             │←──────── MTU (1500) ──────────►│

MSS:  ┌──────────────────────────────────────────┐
      │ ETH │ IP │ TCP │     DATOS              │ FCS │
      └──────────────────────────────────────────┘
                        │←── MSS (1460) ──────►│

MTU = tamaño máximo del paquete completo (capa de red)
MSS = tamaño máximo de datos de aplicación dentro del segmento TCP
```

---

## 9. Ver encapsulación en la práctica

### Con tcpdump

```bash
# Capturar un ping y ver las capas
sudo tcpdump -i enp0s3 -n -v icmp -c 2
```

Salida (simplificada):

```
08:30:01 IP (tos 0x0, ttl 64, id 12345, offset 0, flags [DF],
  proto ICMP (1), length 84)
  192.168.1.10 > 8.8.8.8: ICMP echo request, id 1234, seq 1, length 64
         │              │        │           │
         └── IP hdr ────┘        └── ICMP ───┘
         (capa de red)           (payload)
```

### Con tcpdump y detalle hexadecimal

```bash
# Ver los bytes reales del frame
sudo tcpdump -i enp0s3 -XX -c 1 icmp
```

```
  0x0000: aa11 bb22 cc33 dd44 ee55 ff66 0800 4500  ..".3.D.U.f..E.
          ──── MAC dst ────  ── MAC src ──  ────    ──── IP header
          (capa enlace)      (capa enlace)  EType   (capa red)
```

### Con Wireshark (si tienes GUI)

Wireshark muestra cada capa como un árbol expandible — la mejor forma visual de ver
la encapsulación.

---

## 10. Errores comunes

### Error 1: Confundir MTU con el tamaño total del frame

```
MTU = 1500 bytes     ← payload del frame (paquete IP completo)
Frame total = 1518 bytes  ← 14 (ETH header) + 1500 (payload) + 4 (FCS)

El MTU NO incluye el header Ethernet ni el FCS.
```

### Error 2: Pensar que fragmentación es normal

```
"Mis paquetes se fragmentan, no pasa nada"

Sí pasa:
- Cada fragmento perdido fuerza retransmisión del paquete completo
- Los firewalls tienen problemas con fragmentos (sin puertos en frag 2+)
- Rendimiento degradado

La fragmentación debe ser la excepción, no la norma.
Path MTU Discovery existe para evitarla.
```

### Error 3: Bloquear todo ICMP en el firewall

```bash
# "ICMP es inseguro, bloquearlo todo"
sudo iptables -A INPUT -p icmp -j DROP    ← MALA IDEA
```

Bloquear ICMP rompe Path MTU Discovery, ping, y traceroute. En producción, se permiten
al menos los tipos esenciales:

```bash
# Permitir los tipos ICMP necesarios
sudo iptables -A INPUT -p icmp --icmp-type echo-request -j ACCEPT
sudo iptables -A INPUT -p icmp --icmp-type echo-reply -j ACCEPT
sudo iptables -A INPUT -p icmp --icmp-type destination-unreachable -j ACCEPT
sudo iptables -A INPUT -p icmp --icmp-type time-exceeded -j ACCEPT
# "destination-unreachable" incluye "Packet Too Big" → necesario para PMTUD
```

### Error 4: Configurar jumbo frames solo en un extremo

```
PC (MTU 9000) ──── Switch ──── Servidor (MTU 1500)

Paquetes de 9000 bytes del PC:
  - Si DF=1: se descartan, ICMP "Too Big" → conexión lenta
  - Si DF=0: fragmentación masiva → rendimiento horrible

TODOS los dispositivos en la ruta deben tener el mismo MTU.
```

### Error 5: Calcular mal el MSS

```
"MSS = MTU - 20 (IP header)"  ← INCORRECTO (falta restar TCP header)

Correcto:
  MSS = MTU - IP header - TCP header
  MSS = 1500 - 20 - 20 = 1460

Con opciones TCP (timestamps):
  MSS = 1500 - 20 - 32 = 1448
```

---

## 11. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║              ENCAPSULACIÓN — REFERENCIA RÁPIDA                       ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                      ║
║  ENCAPSULACIÓN (emisor, de arriba hacia abajo)                       ║
║  App genera datos → TCP añade header → IP añade header               ║
║  → Ethernet añade header + trailer → cable                          ║
║                                                                      ║
║  DESENCAPSULACIÓN (receptor, de abajo hacia arriba)                  ║
║  Cable → Ethernet quita header/trailer → IP quita header             ║
║  → TCP quita header → App recibe datos                               ║
║                                                                      ║
║  NOMBRES DE PDU POR CAPA                                             ║
║  Aplicación:  Datos / Mensaje                                        ║
║  Transporte:  Segmento (TCP) / Datagrama (UDP)                       ║
║  Red:         Paquete                                                ║
║  Enlace:      Frame                                                  ║
║                                                                      ║
║  TAMAÑOS DE HEADER                                                   ║
║  Ethernet:  14 bytes (+ 4 FCS trailer)                               ║
║  IPv4:      20 bytes (mínimo, sin opciones)                          ║
║  TCP:       20 bytes (mínimo, sin opciones)                          ║
║  UDP:       8 bytes                                                  ║
║                                                                      ║
║  MTU Y MSS                                                           ║
║  MTU estándar Ethernet:  1500 bytes (payload del frame)              ║
║  MSS = MTU - 20 (IP) - 20 (TCP) = 1460 bytes                        ║
║  Frame total = 14 + MTU + 4 = 1518 bytes                             ║
║                                                                      ║
║  COMANDOS                                                            ║
║  ip link show                     Ver MTU actual                     ║
║  ip link set dev mtu N            Cambiar MTU                        ║
║  tracepath <host>                 Descubrir Path MTU                 ║
║  ping -M do -s <size> <host>     Probar MTU manualmente              ║
║  tcpdump -XX -c 1                Ver bytes del frame                 ║
║                                                                      ║
║  FRAGMENTACIÓN                                                       ║
║  DF=1 → no fragmentar → ICMP "Too Big" si excede MTU                ║
║  DF=0 → fragmentar → cada fragmento viaja independiente              ║
║  Evitar fragmentación: Path MTU Discovery + MSS negotiation          ║
║                                                                      ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 12. Ejercicios

### Ejercicio 1: Observar la encapsulación con tcpdump

1. Abre dos terminales en tu VM
2. En la terminal 1, inicia una captura:
   ```bash
   sudo tcpdump -i enp0s3 -n -v -c 4 icmp
   ```
3. En la terminal 2, ejecuta:
   ```bash
   ping -c 2 8.8.8.8
   ```
4. Observa la salida de tcpdump e identifica para cada paquete:
   - La dirección IP origen y destino (capa de red)
   - El protocolo (ICMP)
   - El TTL
   - El tamaño total (`length`)
5. Ahora repite la captura con `-XX` para ver los bytes crudos:
   ```bash
   sudo tcpdump -i enp0s3 -XX -n -c 1 icmp
   ```
6. En la salida hexadecimal, identifica:
   - Los primeros 6 bytes: MAC destino
   - Los siguientes 6 bytes: MAC origen
   - Los 2 bytes siguientes: EtherType (¿qué valor ves?)

**Pregunta de reflexión**: ¿por qué tcpdump muestra información de la capa de red
(IP) y transporte pero normalmente oculta la capa de enlace? ¿Qué flag necesitas
para verla? ¿En qué capa opera tcpdump realmente?

### Ejercicio 2: Descubrir el Path MTU

1. Descubre el MTU hacia Google:
   ```bash
   tracepath 8.8.8.8
   ```
   Observa la línea `pmtu` — ¿cuál es el MTU mínimo en la ruta?

2. Prueba el límite exacto con ping:
   ```bash
   # Prueba con payload de 1472 (1472 + 8 ICMP + 20 IP = 1500)
   ping -M do -s 1472 -c 1 8.8.8.8

   # Prueba con payload de 1473 (1 byte más que el MTU)
   ping -M do -s 1473 -c 1 8.8.8.8
   ```

**Pregunta de predicción**: ¿qué mensaje esperas con `-s 1473`? ¿Por qué usamos
1472 y no 1500 como tamaño de prueba?

3. Si tu red tiene VPN o tunneling, es posible que el MTU sea menor que 1500. Prueba
   valores descendentes hasta encontrar el máximo que funciona:
   ```bash
   ping -M do -s 1400 -c 1 8.8.8.8
   ping -M do -s 1450 -c 1 8.8.8.8
   # etc.
   ```

**Pregunta de reflexión**: ¿por qué una VPN reduce el MTU? Pista: piensa en qué le
pasa a un paquete cuando se encapsula dentro de otro paquete (encapsulación sobre
encapsulación).

### Ejercicio 3: Calcular overhead de encapsulación

Sin ejecutar comandos, calcula:

1. Quieres enviar un archivo de exactamente 10000 bytes por TCP sobre Ethernet
   (MTU 1500). Asume headers sin opciones (IP=20B, TCP=20B):
   - ¿Cuántos segmentos TCP se necesitan?
   - ¿Cuántos bytes de headers TCP+IP se añaden en total?
   - ¿Cuántos bytes viajan realmente por el cable (incluyendo headers Ethernet)?
   - ¿Qué porcentaje del tráfico total es overhead?

2. Repite el cálculo para jumbo frames (MTU 9000).

3. Compara los porcentajes de overhead.

**Pregunta de reflexión**: ¿por qué los data centers internos usan jumbo frames pero
Internet no? ¿Qué pasaría si un solo enlace en la ruta no soporta jumbo frames?
