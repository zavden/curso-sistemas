# UDP: Sin Conexión, Sin Garantía, Cuándo Preferir sobre TCP

## Índice

1. [Qué es UDP](#1-qué-es-udp)
2. [El header UDP](#2-el-header-udp)
3. [Comparación directa con TCP](#3-comparación-directa-con-tcp)
4. [Cuándo usar UDP sobre TCP](#4-cuándo-usar-udp-sobre-tcp)
5. [Protocolos que usan UDP](#5-protocolos-que-usan-udp)
6. [Comportamiento ante pérdida](#6-comportamiento-ante-pérdida)
7. [UDP y firewalls](#7-udp-y-firewalls)
8. [UDP en Linux](#8-udp-en-linux)
9. [QUIC: UDP como base para transporte fiable](#9-quic-udp-como-base-para-transporte-fiable)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué es UDP

UDP (User Datagram Protocol) es el protocolo de transporte **minimalista** de la pila
TCP/IP. Proporciona solo dos cosas: **multiplexación por puertos** y un **checksum**
opcional. Todo lo demás — fiabilidad, orden, control de flujo — es responsabilidad
de la aplicación.

```
TCP:  Conexión + Fiabilidad + Orden + Control de flujo + Control de congestión
      ──────────────────────────────────────────────────────────────────────────
      Complejo pero seguro

UDP:  Puertos + Checksum
      ──────────────────
      Simple pero sin garantías
```

### La filosofía de UDP

```
TCP dice:  "Te entrego TODO, en ORDEN, SIN errores, aunque tarde"
UDP dice:  "Te envío esto. Si llega, bien. Si no, sigue adelante"
```

No es que UDP sea "peor" que TCP. Es una herramienta diferente para problemas
diferentes. TCP añade overhead (handshake, ACKs, retransmisiones) que para algunas
aplicaciones es peor que perder un paquete ocasionalmente.

---

## 2. El header UDP

### Estructura (8 bytes — el más simple posible)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
├───────────────────────────────┬───────────────────────────────────┤
│     Source Port (16 bits)     │   Destination Port (16 bits)      │
├───────────────────────────────┼───────────────────────────────────┤
│     Length (16 bits)          │   Checksum (16 bits)              │
└───────────────────────────────┴───────────────────────────────────┘
│◄──────────────── 8 bytes ────────────────────────────────────────►│
```

### Comparación de tamaño de header

```
UDP header:  8 bytes
TCP header:  20-60 bytes (20 mínimo + opciones)

Overhead por paquete:
  TCP: 20 bytes de header + ACKs separados + handshake
  UDP: 8 bytes de header, nada más
```

### Campos

| Campo | Tamaño | Descripción |
|---|---|---|
| **Source Port** | 16 bits | Puerto del emisor (opcional, puede ser 0) |
| **Destination Port** | 16 bits | Puerto del receptor |
| **Length** | 16 bits | Tamaño total del datagrama (header + datos) |
| **Checksum** | 16 bits | Verificación de integridad (obligatorio en IPv6, opcional en IPv4) |

### Lo que NO tiene

```
UDP no tiene:
  ✗ Sequence numbers      → no puede reordenar
  ✗ Acknowledgments       → no sabe si llegó
  ✗ Window size           → no controla flujo
  ✗ Flags (SYN/FIN/RST)   → no hay conexión ni cierre
  ✗ Retransmission timer  → no retransmite
  ✗ Connection state      → no mantiene estado
```

---

## 3. Comparación directa con TCP

```
┌──────────────────────────┬─────────────────┬─────────────────┐
│ Característica            │ TCP             │ UDP             │
├──────────────────────────┼─────────────────┼─────────────────┤
│ Conexión                  │ Sí (handshake)  │ No              │
│ Fiabilidad                │ Sí (ACK+retx)   │ No              │
│ Orden de entrega          │ Sí (seq nums)   │ No              │
│ Duplicados                │ Detectados      │ Posibles        │
│ Control de flujo          │ Sí (window)     │ No              │
│ Control de congestión     │ Sí (cwnd)       │ No              │
│ Header                    │ 20-60 bytes     │ 8 bytes         │
│ Latencia de inicio        │ 1 RTT (handshake)│ 0 (envío directo)│
│ Unidad de datos           │ Stream de bytes │ Datagramas      │
│ Preserva límites de msg   │ No              │ Sí              │
│ Broadcast/Multicast       │ No              │ Sí              │
└──────────────────────────┴─────────────────┴─────────────────┘
```

### UDP preserva límites de mensaje

A diferencia de TCP (que es un stream continuo), UDP preserva los límites de cada
datagrama:

```
TCP (stream):
  send("Hello")  +  send("World")
  recv() → "HelloWorld" o "Hel" o "HelloWor" (cualquier división)

UDP (datagramas):
  sendto("Hello")  +  sendto("World")
  recvfrom() → "Hello"   (siempre un datagrama completo)
  recvfrom() → "World"   (siempre un datagrama completo)
  (o uno se pierde, pero nunca se mezclan)
```

---

## 4. Cuándo usar UDP sobre TCP

### UDP es mejor cuando

```
1. LA VELOCIDAD IMPORTA MÁS QUE LA FIABILIDAD
   VoIP: si un paquete de voz se pierde, retransmitirlo llega tarde
         → es mejor reproducir silencio que esperar el paquete retrasado

2. LOS DATOS SE VUELVEN OBSOLETOS RÁPIDAMENTE
   Streaming de video: el frame del segundo 5 no sirve si ya estás
         en el segundo 7 → retransmitir es contraproducente

3. NECESITAS BROADCAST O MULTICAST
   DHCP Discover: el cliente no sabe la IP del servidor
         → envía broadcast, imposible con TCP (requiere IP destino)

4. CONSULTAS PEQUEÑAS Y RÁPIDAS
   DNS: una pregunta, una respuesta, cada una cabe en un datagrama
         → el overhead de un handshake TCP triplicaría el tiempo

5. LA APLICACIÓN IMPLEMENTA SU PROPIO CONTROL
   Juegos online: la app sabe qué datos son críticos y cuáles no
         → implementa fiabilidad selectiva sobre UDP
```

### TCP es mejor cuando

```
1. TODOS LOS DATOS DEBEN LLEGAR
   Transferencia de archivos, email, web pages
   → un byte perdido corrompe todo

2. EL ORDEN IMPORTA
   Bases de datos, SSH, protocolos de comandos
   → ejecutar comandos fuera de orden es catastrófico

3. NO QUIERES IMPLEMENTAR FIABILIDAD TÚ MISMO
   La mayoría de aplicaciones
   → TCP ya resuelve retransmisión, orden, control de flujo
```

### Diagrama de decisión

```
¿Necesitas que TODOS los datos lleguen sin excepción?
├── Sí → TCP (o QUIC)
└── No
    ├── ¿Datos se vuelven obsoletos rápidamente?
    │   └── Sí → UDP (streaming, VoIP, gaming)
    ├── ¿Necesitas broadcast/multicast?
    │   └── Sí → UDP (DHCP, mDNS, service discovery)
    ├── ¿Consultas simples request/response?
    │   └── Sí → UDP (DNS, NTP, SNMP)
    └── ¿Quieres control total sobre retransmisiones?
        └── Sí → UDP + tu protocolo encima (QUIC, game engines)
```

---

## 5. Protocolos que usan UDP

### Protocolos principales

```
Protocolo   Puerto    Por qué UDP
─────────── ───────── ──────────────────────────────────────────
DNS         53        Consultas pequeñas, baja latencia
                      (usa TCP para transferencias de zona y
                       respuestas > 512 bytes)

DHCP        67/68     Broadcast (el cliente no tiene IP aún)

NTP         123       Sincronización de tiempo — un paquete basta

SNMP        161/162   Monitorización — polling de métricas

TFTP        69        Transferencia simple (implementa su
                      propia fiabilidad mínima)

syslog      514       Logs remotos — tolerante a pérdida

RTP         variable  Audio/video en tiempo real (VoIP, streaming)

QUIC        443       HTTP/3 — fiabilidad implementada sobre UDP

mDNS        5353      Service discovery en red local (Avahi/Bonjour)

WireGuard   51820     VPN — gestiona su propia fiabilidad
```

### DNS: el ejemplo perfecto de cuándo UDP es mejor

```
Consulta DNS con UDP:
  Cliente → [pregunta: 40 bytes] → Servidor DNS
  Servidor → [respuesta: ~100 bytes] → Cliente
  Total: 2 paquetes, ~0.5ms

Consulta DNS con TCP:
  Cliente → SYN → Servidor
  Servidor → SYN+ACK → Cliente
  Cliente → ACK → Servidor
  Cliente → [pregunta] → Servidor
  Servidor → [respuesta] → Cliente
  Cliente → FIN → Servidor
  Servidor → FIN+ACK → Cliente
  Cliente → ACK → Servidor
  Total: 8+ paquetes, ~2ms

UDP es 4x más rápido para una simple consulta DNS.
```

---

## 6. Comportamiento ante pérdida

### UDP no hace nada

```
Emisor                              Receptor
  │                                     │
  │  Datagrama 1 ──────────────────►    │  ✔ Recibido
  │  Datagrama 2 ──────────────────►    │  (se pierde)
  │  Datagrama 3 ──────────────────►    │  ✔ Recibido
  │                                     │
  │  El emisor NO sabe que el 2 se perdió
  │  El receptor recibe 1 y 3, nunca recibe 2
  │  Nadie retransmite nada
```

### Qué hacen las aplicaciones

Si la aplicación necesita fiabilidad sobre UDP, la implementa ella misma:

```
DNS:
  Envía consulta → espera respuesta → timeout → reenvía la consulta
  (fiabilidad simple a nivel de aplicación)

TFTP:
  Envía bloque → espera ACK → timeout → reenvía
  (fiabilidad bloque a bloque)

QUIC:
  Implementa stream multiplexing, retransmisión selectiva,
  control de congestión — esencialmente TCP reimplementado
  sobre UDP, pero sin head-of-line blocking

Juegos online:
  Datos críticos (posición del jugador): con seq number + ACK propio
  Datos no críticos (efecto de sonido): fire-and-forget
  (fiabilidad selectiva)
```

### Reordenamiento

```
Emisor envía: Datagrama A, B, C (en ese orden)
Red entrega:  Datagrama A, C, B (reordenados)

UDP entrega a la aplicación: A, C, B
  → La aplicación recibe datagramas fuera de orden
  → Si el orden importa, la app debe añadir seq numbers propios
```

---

## 7. UDP y firewalls

### El problema de los firewalls stateful con UDP

TCP tiene un handshake claro: el firewall ve el SYN, sabe que es una conexión nueva,
y puede rastrear su estado. UDP no tiene handshake — el firewall tiene que adivinar.

```
TCP:
  Firewall ve SYN → "nueva conexión, permitir respuesta"
  Firewall ve FIN → "conexión cerrada, limpiar estado"
  → Estado claro del principio al final

UDP:
  Firewall ve un datagrama saliente → "¿es una 'conexión'?"
  Crea una entrada temporal (src:port → dst:port)
  Timeout (30-120 segundos) → elimina la entrada
  → Si la respuesta tarda más que el timeout, se bloquea
```

### ICMP Port Unreachable

Cuando un datagrama UDP llega a un puerto donde nadie escucha:

```
Emisor ──[UDP dst port 9999]──► Host destino
                                  Puerto cerrado, nadie escucha
         ◄── ICMP Type 3 Code 3 ──
              "Port Unreachable"

(Con TCP, el host respondería RST)
```

Si el firewall bloquea ICMP, el emisor nunca sabe que el puerto está cerrado — el
datagrama se pierde silenciosamente.

### Port scanning y UDP

```
Escanear puertos UDP es más lento y menos fiable que TCP:

  TCP scan:    SYN → SYN+ACK = abierto, RST = cerrado     (claro)
  UDP scan:    Datagrama → silencio = ¿abierto? ¿filtrado? (ambiguo)
                         → ICMP unreachable = cerrado       (claro)

  nmap necesita esperar el timeout para cada puerto UDP → muy lento
```

---

## 8. UDP en Linux

### Ver sockets UDP

```bash
# Sockets UDP escuchando
ss -ulnp
# State   Recv-Q  Send-Q  Local Address:Port   Peer Address:Port  Process
# UNCONN  0       0       0.0.0.0:68            0.0.0.0:*          dhclient
# UNCONN  0       0       127.0.0.53%lo:53      0.0.0.0:*          systemd-resolve

# Todos los sockets UDP (escuchando + activos)
ss -uanp
```

> **Nota**: UDP no tiene estados como ESTABLISHED o LISTEN. `ss` muestra `UNCONN`
> (unconnected) para todos los sockets UDP, aunque estén "conectados" con `connect()`.

### Enviar y recibir UDP con herramientas básicas

```bash
# Escuchar en un puerto UDP
nc -u -l 9999

# Enviar un datagrama UDP
echo "hello" | nc -u localhost 9999

# Enviar UDP con socat (más flexible)
echo "test" | socat - UDP:localhost:9999
```

### Capturar tráfico UDP

```bash
# Capturar todo UDP
sudo tcpdump -i enp0s3 -n udp

# Solo DNS (UDP port 53)
sudo tcpdump -i enp0s3 -n udp port 53

# Solo DHCP
sudo tcpdump -i enp0s3 -n udp port 67 or udp port 68
```

### Buffers UDP del kernel

```bash
# Tamaño de buffers de recepción UDP
sysctl net.core.rmem_default    # Buffer default por socket
# 212992

sysctl net.core.rmem_max        # Buffer máximo permitido
# 212992

# Si una aplicación UDP recibe ráfagas rápidas, aumentar el buffer:
sudo sysctl -w net.core.rmem_max=26214400     # 25 MB
```

### Estadísticas UDP

```bash
# Errores y estadísticas UDP del kernel
cat /proc/net/snmp | grep Udp
# Udp: InDatagrams NoPorts InErrors OutDatagrams RcvbufErrors SndbufErrors

# O con ss
ss -s
# UDP: 5 (estab 0, closed 0, orphaned 0, timewait 0)
```

Campos importantes en `/proc/net/snmp`:
- **RcvbufErrors**: datagramas descartados porque el buffer de recepción estaba lleno
  → la app no lee lo suficientemente rápido o el buffer es muy pequeño
- **InErrors**: datagramas con errores (checksum incorrecto, etc.)

---

## 9. QUIC: UDP como base para transporte fiable

### Por qué reinventar TCP sobre UDP

```
Problemas de TCP que motivaron QUIC:

1. HEAD-OF-LINE BLOCKING:
   TCP entrega bytes en orden. Si un paquete se pierde, todos los
   posteriores esperan la retransmisión — aunque sean de streams
   independientes (HTTP/2 multiplexa streams sobre un solo TCP).

   TCP con HTTP/2:
     Stream A: ████ ░░░░ ████   ← stream A espera al paquete perdido
     Stream B: ████ ░░░░ ████   ← stream B también espera (¡no debería!)
     Stream C: ████ ░░░░ ████   ← todos bloqueados
                    │
                    └── Paquete perdido (de stream A)

   QUIC con HTTP/3:
     Stream A: ████ ░░░░ ████   ← solo stream A espera
     Stream B: ████████████████   ← stream B NO se bloquea
     Stream C: ████████████████   ← stream C NO se bloquea

2. HANDSHAKE LENTO:
   TCP: 1 RTT (handshake) + 1 RTT (TLS handshake) = 2 RTT antes de datos
   QUIC: 1 RTT (handshake + crypto integrados) o 0 RTT (reconexión)

3. OSSIFICACIÓN:
   TCP está en el kernel → cambiar su comportamiento requiere actualizar
   millones de kernels y middleboxes. QUIC está en userspace → se actualiza
   con la aplicación.
```

### QUIC sobre UDP

```
┌────────────────────────────────────────┐
│  HTTP/3 (aplicación)                    │
├────────────────────────────────────────┤
│  QUIC (transporte fiable + crypto)      │  ← Implementa:
│  - Streams multiplexados                │    retransmisión, orden,
│  - TLS 1.3 integrado                   │    control de congestión,
│  - Migración de conexión               │    cifrado — TODO
├────────────────────────────────────────┤
│  UDP (solo puertos + checksum)          │  ← Mínimo necesario
├────────────────────────────────────────┤
│  IP                                     │
└────────────────────────────────────────┘

QUIC usa UDP solo como "contenedor" para pasar por los firewalls
y NAT existentes (que ya saben manejar UDP).
```

### Verificar si tu navegador usa QUIC

```bash
# Muchos sitios de Google y Cloudflare usan QUIC (HTTP/3)
curl --http3 https://www.google.com -I 2>&1 | grep HTTP
# HTTP/3 200

# Ver tráfico QUIC (UDP 443)
sudo tcpdump -i enp0s3 -n udp port 443
```

---

## 10. Errores comunes

### Error 1: "UDP no es fiable, nunca lo uses"

```
UDP no es fiable a nivel de PROTOCOLO.
Eso no significa que la comunicación sea infiable.

En una LAN local, la pérdida de paquetes UDP es prácticamente 0%.
En Internet, la pérdida típica es 0.1-1%.

DNS usa UDP y funciona perfectamente para miles de millones de consultas
diarias. La aplicación maneja los casos raros de pérdida.
```

### Error 2: Enviar datagramas más grandes que el MTU

```
UDP datagrama de 5000 bytes sobre Ethernet (MTU 1500):

  IP fragmenta el datagrama en 4 fragmentos:
  Frag 1: 1480 bytes
  Frag 2: 1480 bytes
  Frag 3: 1480 bytes
  Frag 4: 560 bytes

  Si se pierde UN fragmento → todo el datagrama se pierde
  (UDP no retransmite)

Regla práctica: mantener datagramas UDP bajo el MTU.
  MTU 1500 - 20 (IP) - 8 (UDP) = 1472 bytes máximo recomendado.
  DNS limita a 512 bytes (o 4096 con EDNS).
```

### Error 3: No manejar el tamaño de buffer en aplicaciones de alto tráfico

```bash
# Verificar si se están perdiendo datagramas por buffer lleno
cat /proc/net/snmp | grep Udp
# Si RcvbufErrors > 0, la app no lee lo suficientemente rápido

# Solución 1: app usa buffer más grande
setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));

# Solución 2: kernel permite buffers más grandes
sudo sysctl -w net.core.rmem_max=26214400
```

### Error 4: Asumir que UDP mantiene el orden

```
Emisor: send(A), send(B), send(C)
Receptor podría recibir: A, C, B
                         B, A, C
                         A, C    (B perdido)

Si tu aplicación necesita orden, añade sequence numbers.
```

### Error 5: Olvidar que UDP no tiene control de congestión

```
TCP reduce su velocidad cuando detecta congestión.
UDP NO. Si envías datagramas tan rápido como puedes:

  - Saturas la red
  - Perjudicas a TODAS las conexiones (TCP y UDP) en el enlace
  - Los routers descartan paquetes indiscriminadamente
  - Tu propia tasa de pérdida aumenta

Si implementas un protocolo sobre UDP, DEBES implementar
alguna forma de control de congestión (como hace QUIC).
```

---

## 11. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║                       UDP — REFERENCIA RÁPIDA                        ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                      ║
║  CARACTERÍSTICAS                                                     ║
║  Sin conexión (no handshake)                                         ║
║  Sin fiabilidad (no ACK, no retransmisión)                           ║
║  Sin orden garantizado                                               ║
║  Sin control de flujo ni congestión                                  ║
║  Preserva límites de mensaje (datagramas)                            ║
║  Header mínimo: 8 bytes                                              ║
║  Soporta broadcast y multicast                                       ║
║                                                                      ║
║  HEADER UDP                                                          ║
║  ┌─────────────┬─────────────┐                                       ║
║  │  Src Port   │  Dst Port   │  4 bytes                              ║
║  ├─────────────┼─────────────┤                                       ║
║  │  Length      │  Checksum   │  4 bytes                              ║
║  └─────────────┴─────────────┘                                       ║
║  Total: 8 bytes (vs 20+ de TCP)                                      ║
║                                                                      ║
║  PROTOCOLOS PRINCIPALES                                              ║
║  DNS:       53     (consultas rápidas)                                ║
║  DHCP:      67/68  (broadcast necesario)                              ║
║  NTP:       123    (sincronización de tiempo)                        ║
║  SNMP:      161    (monitorización)                                  ║
║  syslog:    514    (logging remoto)                                   ║
║  QUIC:      443    (HTTP/3, fiabilidad propia)                       ║
║  WireGuard: 51820  (VPN)                                             ║
║                                                                      ║
║  CUÁNDO USAR UDP                                                     ║
║  ✔ Datos que caducan rápido (streaming, VoIP, gaming)                ║
║  ✔ Broadcast/multicast necesario (DHCP, mDNS)                       ║
║  ✔ Consultas simples (DNS, NTP)                                      ║
║  ✔ Control total sobre retransmisiones (QUIC, game protocols)        ║
║                                                                      ║
║  CUÁNDO NO USAR UDP                                                  ║
║  ✗ Todos los datos deben llegar (archivos, email, web)               ║
║  ✗ El orden es crítico (bases de datos, SSH)                         ║
║  ✗ No quieres implementar fiabilidad tú mismo                       ║
║                                                                      ║
║  COMANDOS LINUX                                                      ║
║  ss -ulnp                       Sockets UDP escuchando               ║
║  ss -uanp                       Todos los sockets UDP                ║
║  ss -s                          Estadísticas por protocolo            ║
║  tcpdump -n udp port 53         Capturar UDP en puerto 53            ║
║  nc -u -l 9999                  Escuchar UDP en puerto 9999          ║
║  echo "test" | nc -u host 9999  Enviar datagrama UDP                 ║
║  cat /proc/net/snmp | grep Udp  Estadísticas UDP del kernel          ║
║                                                                      ║
║  TAMAÑO RECOMENDADO                                                  ║
║  Máx sin fragmentación: 1472 bytes (1500 MTU - 20 IP - 8 UDP)       ║
║  DNS clásico: 512 bytes; EDNS: 4096 bytes                            ║
║                                                                      ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 12. Ejercicios

### Ejercicio 1: UDP vs TCP en DNS

1. Captura una consulta DNS normal (UDP):
   ```bash
   sudo tcpdump -i enp0s3 -n udp port 53 -c 2 &
   dig example.com
   ```
   Observa: ¿cuántos paquetes se intercambian? (pregunta + respuesta = 2)

2. Ahora fuerza la consulta por TCP:
   ```bash
   sudo tcpdump -i enp0s3 -n tcp port 53 -c 10 &
   dig +tcp example.com
   ```
   Observa: ¿cuántos paquetes se intercambian ahora? (handshake + pregunta +
   respuesta + cierre)

3. Compara los tiempos de respuesta:
   ```bash
   dig example.com | grep "Query time"
   dig +tcp example.com | grep "Query time"
   ```

**Pregunta de reflexión**: si la consulta TCP requiere ~8 paquetes y la UDP solo 2,
¿por qué DNS usa TCP en algunos casos? ¿Cuándo es obligatorio TCP para DNS?

### Ejercicio 2: Observar pérdida y reordenamiento

1. Abre un receptor UDP:
   ```bash
   nc -u -l 9999
   ```

2. En otra terminal, envía varios datagramas:
   ```bash
   for i in $(seq 1 10); do echo "mensaje $i" | nc -u -w 0 localhost 9999; done
   ```

3. Observa en el receptor: ¿llegaron todos? ¿En orden?
   (En localhost la pérdida es ~0%, pero el ejercicio muestra el patrón)

4. Ahora comprueba las estadísticas UDP:
   ```bash
   cat /proc/net/snmp | grep Udp
   ```
   Identifica InDatagrams y RcvbufErrors.

**Pregunta de reflexión**: en localhost la pérdida es prácticamente 0%. ¿Por qué
en una red real podría haber pérdida? ¿Quién decide descartar un datagrama: el
emisor, un router intermedio, o el receptor?

### Ejercicio 3: Comparar comportamiento de firewall con TCP y UDP

1. Verifica qué puertos TCP están abiertos en tu máquina:
   ```bash
   ss -tlnp
   ```

2. Verifica qué puertos UDP están abiertos:
   ```bash
   ss -ulnp
   ```

3. Intenta conectarte a un puerto TCP cerrado:
   ```bash
   nc -v -z localhost 9998
   # Connection refused (RST inmediato)
   ```

4. Intenta enviar a un puerto UDP cerrado:
   ```bash
   nc -v -u -z localhost 9998
   # ... silencio ...
   ```

5. Captura el ICMP "Port Unreachable":
   ```bash
   sudo tcpdump -i lo -n icmp -c 1 &
   echo "test" | nc -u -w 1 localhost 9998
   ```

**Pregunta de predicción**: cuando envías a un puerto TCP cerrado, recibes RST
inmediatamente. Cuando envías a un puerto UDP cerrado, ¿qué recibes? ¿Es el
kernel o la aplicación quien responde en cada caso?

**Pregunta de reflexión**: si estás escaneando puertos remotos con `nmap`, ¿por qué
el escaneo UDP es mucho más lento que el TCP? ¿Qué tiene que hacer `nmap` cuando no
recibe respuesta de un puerto UDP — asumir abierto o cerrado?
