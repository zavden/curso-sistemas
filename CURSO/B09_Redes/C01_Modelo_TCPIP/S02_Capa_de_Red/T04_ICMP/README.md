# ICMP: Ping, Traceroute, Mensajes de Error y Path MTU Discovery

## Índice

1. [Qué es ICMP](#1-qué-es-icmp)
2. [Formato del mensaje ICMP](#2-formato-del-mensaje-icmp)
3. [Tipos y códigos](#3-tipos-y-códigos)
4. [ping (Echo Request / Echo Reply)](#4-ping-echo-request--echo-reply)
5. [traceroute](#5-traceroute)
6. [Mensajes de error ICMP](#6-mensajes-de-error-icmp)
7. [Path MTU Discovery](#7-path-mtu-discovery)
8. [ICMPv6](#8-icmpv6)
9. [ICMP y firewalls](#9-icmp-y-firewalls)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué es ICMP

ICMP (Internet Control Message Protocol) es el protocolo de **diagnóstico y control**
de la capa de red. No transporta datos de aplicaciones — transporta información
**sobre** la red: errores, diagnósticos y descubrimiento de rutas.

```
Capa de aplicación:  HTTP, SSH, DNS      ← datos de usuario
Capa de transporte:  TCP, UDP            ← entrega de datos
Capa de red:         IP + ICMP           ← ICMP viaja DENTRO de paquetes IP
                           ────
                           Protocolo de control, no de datos
```

### Posición en el modelo

ICMP es un protocolo de capa de red, pero viaja **encapsulado** dentro de paquetes IP:

```
┌──────┬──────┬──────────────────────┐
│ ETH  │  IP  │    Mensaje ICMP      │
│ hdr  │ hdr  │                      │
│      │proto │                      │
│      │ = 1  │                      │
└──────┴──────┴──────────────────────┘
         │
         └── Protocol field = 1 (ICMP)
             (6 = TCP, 17 = UDP)
```

### Qué hace ICMP

```
1. Diagnóstico:     ping (¿estás vivo?), traceroute (¿qué ruta siguen?)
2. Errores:         "Destino inalcanzable", "TTL expirado"
3. Descubrimiento:  Path MTU Discovery ("paquete demasiado grande")
4. Redirección:     "Hay una ruta mejor por otro router"
```

ICMP no establece conexiones. Cada mensaje es independiente.

---

## 2. Formato del mensaje ICMP

### Header ICMP (8 bytes mínimo)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
├───────────────┬───────────────┬───────────────────────────────────┤
│   Type (8b)   │   Code (8b)   │         Checksum (16b)            │
├───────────────┴───────────────┴───────────────────────────────────┤
│                    Contenido variable (32b)                        │
│              (depende del Type: ID+Seq, MTU, puntero, etc.)       │
├───────────────────────────────────────────────────────────────────┤
│                    Data (variable)                                 │
│              (para errores: copia del header IP + 8 bytes del     │
│               paquete que causó el error)                         │
└───────────────────────────────────────────────────────────────────┘
```

- **Type**: categoría del mensaje (echo, destino inalcanzable, etc.)
- **Code**: subcategoría dentro del tipo (red inalcanzable, host inalcanzable, etc.)
- **Checksum**: verificación de integridad del mensaje ICMP
- **Data**: para mensajes de error, incluye parte del paquete original para que el
  emisor identifique qué paquete causó el problema

---

## 3. Tipos y códigos

### Tipos principales de ICMPv4

```
Type  Nombre                        Dirección    Uso
──── ──────────────────────────── ──────────── ─────────────────
  0   Echo Reply                    ← respuesta  Respuesta a ping
  3   Destination Unreachable       ← error       Host/red/puerto inalcanzable
  4   Source Quench (obsoleto)      ← control     Control de congestión (no usar)
  5   Redirect                      ← control     "Hay mejor ruta"
  8   Echo Request                  → pregunta    ping
 11   Time Exceeded                 ← error       TTL = 0 (usado por traceroute)
 12   Parameter Problem             ← error       Header IP malformado
```

### Códigos de Destination Unreachable (Type 3)

```
Code  Significado                    Causa típica
──── ──────────────────────────── ───────────────────────────────
  0   Network Unreachable           Router no tiene ruta a la red destino
  1   Host Unreachable              Router puede llegar a la red pero no al host
  2   Protocol Unreachable          El protocolo solicitado no está disponible
  3   Port Unreachable              Puerto UDP cerrado (sin proceso escuchando)
  4   Fragmentation Needed (DF set) Paquete muy grande, DF=1 → Path MTU Discovery
  6   Destination Network Unknown   La red destino no existe
  9   Network Administratively      Firewall bloqueó el acceso
      Prohibited
 10   Host Administratively         Firewall bloqueó el acceso al host
      Prohibited
 13   Communication Administratively Filtrado por política
      Prohibited
```

### Mensajes query vs error

```
Query (diagnóstico):
  Type 8/0 → Echo Request/Reply (ping)
  → Se generan a petición del usuario

Error (informativo):
  Type 3   → Destination Unreachable
  Type 11  → Time Exceeded
  → Se generan automáticamente por routers/hosts cuando algo falla
  → NUNCA se genera un error ICMP en respuesta a otro error ICMP
    (evita cascadas infinitas)
```

---

## 4. ping (Echo Request / Echo Reply)

### Cómo funciona

```
Tu PC                                    Destino
  │                                        │
  │  Echo Request (Type 8, Code 0)         │
  │  ID=1234, Seq=1, datos="abcdef..."     │
  │───────────────────────────────────────►│
  │                                        │
  │  Echo Reply (Type 0, Code 0)           │
  │  ID=1234, Seq=1, datos="abcdef..."     │
  │◄───────────────────────────────────────│
  │                                        │
  RTT = tiempo entre enviar Request y recibir Reply
```

El campo **ID** identifica la sesión de ping (permite que múltiples pings coexistan).
El campo **Seq** (sequence) se incrementa en cada paquete para detectar pérdida y
reordenamiento.

### Uso básico

```bash
# Ping básico (infinite en Linux, Ctrl+C para parar)
ping 8.8.8.8

# Número fijo de paquetes
ping -c 5 8.8.8.8

# Intervalo entre paquetes (default: 1 segundo)
ping -i 0.2 -c 10 8.8.8.8    # cada 0.2 segundos

# Tamaño del payload (default: 56 bytes → 64 con header ICMP)
ping -s 1000 -c 3 8.8.8.8

# Sin resolución DNS inversa (más rápido)
ping -n -c 3 8.8.8.8

# Ping flood (requiere root, para testing de red)
sudo ping -f -c 1000 192.168.1.1
```

### Interpretar la salida

```bash
ping -c 4 8.8.8.8
```

```
PING 8.8.8.8 (8.8.8.8) 56(84) bytes of data.
64 bytes from 8.8.8.8: icmp_seq=1 ttl=118 time=12.3 ms
64 bytes from 8.8.8.8: icmp_seq=2 ttl=118 time=11.8 ms
64 bytes from 8.8.8.8: icmp_seq=3 ttl=118 time=12.1 ms
64 bytes from 8.8.8.8: icmp_seq=4 ttl=118 time=11.9 ms

--- 8.8.8.8 ping statistics ---
4 packets transmitted, 4 received, 0% packet loss, time 3004ms
rtt min/avg/max/mdev = 11.800/12.025/12.300/0.183 ms
```

| Campo | Significado |
|---|---|
| `64 bytes` | Tamaño de la respuesta (56 payload + 8 header ICMP) |
| `icmp_seq=1` | Número de secuencia (detecta pérdida si falta alguno) |
| `ttl=118` | TTL de la respuesta. TTL inicial probable: 128 → 128-118 = 10 saltos |
| `time=12.3 ms` | RTT (Round Trip Time) — latencia ida y vuelta |
| `0% packet loss` | Porcentaje de paquetes perdidos |
| `rtt min/avg/max/mdev` | Estadísticas de latencia. mdev = desviación (jitter) |

### Qué significa cada resultado de ping

```
Respuesta exitosa:       El host está vivo y alcanzable
"Request timeout":       No hubo respuesta (host caído, firewall, o ruta rota)
"Destination unreachable": Un router devolvió un error ICMP (Type 3)
"TTL exceeded":          Un paquete llegó a TTL=0 en un router intermedio
Alto packet loss:        Congestión de red o enlace inestable
RTT variable (jitter):   Red congestionada o enlace inalámbrico inestable
```

---

## 5. traceroute

### Cómo funciona

Traceroute descubre cada router en la ruta al destino explotando el mecanismo de TTL:

```
Paso 1: Enviar paquete con TTL=1
  Tu PC ──[TTL=1]──► Router 1
                      TTL llega a 0 → descarta
                      Router 1 envía ICMP Time Exceeded (Type 11)
                      ◄─── "Soy Router 1 (10.0.0.1)"

Paso 2: Enviar paquete con TTL=2
  Tu PC ──[TTL=2]──► Router 1 ──[TTL=1]──► Router 2
                                             TTL llega a 0
                      ◄─── "Soy Router 2 (10.0.1.1)"

Paso 3: Enviar paquete con TTL=3
  Tu PC ──[TTL=3]──► Router 1 ──► Router 2 ──[TTL=1]──► Router 3
                                                          TTL llega a 0
                      ◄─── "Soy Router 3 (10.0.2.1)"

...continúa hasta que TTL es suficiente para llegar al destino
```

### Variantes en Linux

```bash
# traceroute clásico (puede usar UDP, ICMP, o TCP)
traceroute 8.8.8.8

# Usando ICMP echo (como ping, más probable que pase firewalls)
traceroute -I 8.8.8.8

# Usando TCP al puerto 80 (útil si ICMP/UDP están filtrados)
traceroute -T -p 80 8.8.8.8

# tracepath: no requiere root, descubre MTU de cada salto
tracepath 8.8.8.8

# mtr: traceroute continuo con estadísticas en tiempo real
mtr 8.8.8.8
```

### Interpretar la salida de traceroute

```bash
traceroute -n 8.8.8.8
```

```
traceroute to 8.8.8.8 (8.8.8.8), 30 hops max, 60 byte packets
 1  192.168.1.1     1.234 ms  0.987 ms  1.102 ms
 2  10.0.0.1        5.432 ms  5.211 ms  5.678 ms
 3  * * *
 4  72.14.236.73    12.345 ms  11.987 ms  12.102 ms
 5  8.8.8.8         12.567 ms  12.234 ms  12.445 ms
```

| Elemento | Significado |
|---|---|
| `1` | Número de salto (hop) |
| `192.168.1.1` | IP del router en ese salto |
| `1.234 ms 0.987 ms 1.102 ms` | RTT de 3 intentos (traceroute envía 3 por salto) |
| `* * *` | No hubo respuesta (router no envía ICMP o lo filtra) |

### Interpretar problemas con traceroute

```
Salto con * * *:
  - El router filtra ICMP (no significa que el paquete no pasa)
  - Un * al final (destino) = el host no responde

Latencia que sube en un salto y se mantiene alta:
  Salto 3: 5ms
  Salto 4: 50ms  ← el enlace entre salto 3 y 4 es lento
  Salto 5: 52ms
  → Problema entre router 3 y 4

Latencia que sube en un salto pero baja después:
  Salto 3: 5ms
  Salto 4: 50ms  ← el router 4 prioriza datos sobre ICMP
  Salto 5: 6ms
  → Probablemente no es un problema real (ICMP tiene baja prioridad)
```

### mtr: lo mejor de ambos mundos

```bash
mtr -n 8.8.8.8
```

```
                             My traceroute  [v0.95]
Host                        Loss%   Snt   Last   Avg  Best  Wrst StDev
1. 192.168.1.1               0.0%    50    0.9   1.0   0.8   1.5   0.1
2. 10.0.0.1                  0.0%    50    5.2   5.4   4.8   6.1   0.3
3. ???                       100.0    50    0.0   0.0   0.0   0.0   0.0
4. 72.14.236.73               0.0%    50   12.3  12.5  11.9  13.2   0.2
5. 8.8.8.8                    0.0%    50   12.5  12.7  12.1  13.5   0.3
```

mtr combina traceroute + ping continuo. Muestra:
- **Loss%**: pérdida de paquetes en cada salto
- **Avg**: latencia media
- **StDev**: jitter (variabilidad)

Un salto con loss alto pero saltos posteriores sin loss = el router filtra ICMP, no
hay pérdida real.

---

## 6. Mensajes de error ICMP

### Destination Unreachable (Type 3)

Se genera cuando un paquete no puede llegar a su destino:

```
Escenario: envías UDP al puerto 9999 de un host que no tiene nada en ese puerto

Tu PC ──[UDP dst port 9999]──► Host destino
                                Puerto cerrado
         ◄── ICMP Type 3, Code 3 ──
              "Port Unreachable"

El mensaje ICMP incluye el header IP + primeros 8 bytes del paquete original
para que tu sistema identifique qué conexión falló.
```

### Time Exceeded (Type 11)

```
Code 0: TTL expired in transit
  → Un router decrementó el TTL a 0 y descartó el paquete
  → Es lo que usa traceroute para descubrir routers

Code 1: Fragment reassembly time exceeded
  → El destino recibió algunos fragmentos pero no todos
  → Timeout esperando los fragmentos faltantes → descarta todo
```

### Redirect (Type 5)

```
Escenario:
  Tu PC (192.168.1.10) envía un paquete a 10.0.0.5 vía Router A
  Router A sabe que Router B es mejor opción y están en la misma LAN

Tu PC ──► Router A ──► Router B ──► 10.0.0.5
              │
              └── ICMP Redirect: "Para 10.0.0.5, usa Router B directamente"

Tu PC actualiza su tabla de routing temporalmente.
Próximo paquete a 10.0.0.5 va directo a Router B.
```

> Los redirects son un riesgo de seguridad. En la mayoría de sistemas modernos están
> deshabilitados por defecto:
> ```bash
> sysctl net.ipv4.conf.all.accept_redirects
> # 0 = rechazar (default seguro)
> ```

---

## 7. Path MTU Discovery

### Repaso del mecanismo

Path MTU Discovery usa ICMP Type 3, Code 4 ("Fragmentation Needed and DF was Set")
para descubrir el MTU mínimo de toda la ruta:

```
Tu PC (MTU=1500)     Router (MTU=1400)         Servidor
  │                     │                         │
  │ Paquete 1500B, DF=1 │                         │
  │────────────────────►│                         │
  │                     │ "Paquete muy grande,     │
  │                     │  mi MTU es 1400"         │
  │  ICMP Type 3 Code 4 │                         │
  │  "Frag needed,       │                         │
  │   Next-Hop MTU=1400" │                         │
  │◄────────────────────│                         │
  │                                                │
  │ Paquete 1400B, DF=1 (ajustado)                 │
  │───────────────────────────────────────────────►│
  │                                                │
```

### Ver el Path MTU en Linux

```bash
# El kernel cachea el Path MTU descubierto
ip route get 8.8.8.8
# 8.8.8.8 via 192.168.1.1 dev enp0s3 src 192.168.1.10
#     cache  mtu 1400    ← Path MTU descubierto
```

### PMTU Black Hole

Si un firewall intermedio bloquea ICMP Type 3 Code 4, el emisor nunca recibe la
notificación de MTU reducido:

```
Tu PC ──► Firewall (bloquea ICMP) ──► Router (MTU bajo) ──► Servidor

Paquetes pequeños: ✔ pasan
Paquetes grandes:  ✗ descartados silenciosamente

Síntomas:
  - SSH conecta pero transferir archivos se cuelga
  - Web carga la página pero imágenes grandes no
  - TCP connection stall (la conexión se "congela")
```

### Solución al PMTU Black Hole

```bash
# TCP MSS clamping: forzar un MSS menor en el firewall/router
sudo iptables -t mangle -A FORWARD -p tcp --tcp-flags SYN,RST SYN \
    -j TCPMSS --clamp-mss-to-pmtu

# O fijar un MSS explícito
sudo iptables -t mangle -A FORWARD -p tcp --tcp-flags SYN,RST SYN \
    -j TCPMSS --set-mss 1360
```

---

## 8. ICMPv6

### Diferencias con ICMPv4

ICMPv6 es **mucho más importante** que ICMPv4. En IPv4, ICMP es opcional (puedes
bloquearlo y la red funciona, aunque mal). En IPv6, ICMP es **obligatorio** — sin
ICMPv6 la red no funciona.

```
ICMPv4: diagnóstico + errores (útil pero no crítico)

ICMPv6: diagnóstico + errores + todo lo que hacían ARP, DHCP autoconfig, y más:
  - Neighbor Discovery (reemplaza ARP)
  - Router Discovery (reemplaza parte de DHCP)
  - SLAAC (autoconfiguración de direcciones)
  - DAD (detección de direcciones duplicadas)
  - Path MTU Discovery
```

### Tipos ICMPv6 principales

```
Type   Nombre                          Equivalente IPv4
───── ─────────────────────────────── ──────────────────
  1    Destination Unreachable          Type 3
  2    Packet Too Big                   Type 3 Code 4
  3    Time Exceeded                    Type 11
  4    Parameter Problem                Type 12

128    Echo Request                     Type 8
129    Echo Reply                       Type 0

133    Router Solicitation (RS)         (sin equivalente)
134    Router Advertisement (RA)        (sin equivalente)
135    Neighbor Solicitation (NS)       ARP Request
136    Neighbor Advertisement (NA)      ARP Reply
137    Redirect                         Type 5
```

### Neighbor Discovery Protocol (NDP)

NDP usa ICMPv6 Types 133-137 para reemplazar ARP, DHCP router discovery, y redirect:

```
NS (Type 135) / NA (Type 136):  Equivalente a ARP
  "¿Quién tiene 2001:db8::5?" → "Soy yo, mi MAC es aa:bb:cc:dd:ee:ff"

  Diferencia con ARP:
  - ARP usa broadcast (ff:ff:ff:ff:ff:ff) → todos procesan
  - NDP usa multicast solicitado → solo el dueño procesa
    (multicast group: ff02::1:ffXX:XXXX donde XX:XXXX son los últimos 24 bits de la IP)

RS (Type 133) / RA (Type 134):  Router discovery + SLAAC
  "¿Hay routers?" → "Sí, el prefijo es 2001:db8:1::/64, usadlo"
```

---

## 9. ICMP y firewalls

### El debate: ¿bloquear ICMP?

```
Argumentos para bloquear:
  - Evitar reconocimiento (nmap, ping sweeps)
  - Prevenir ataques históricos (ping of death, smurf — ya mitigados)

Argumentos para NO bloquear:
  - Path MTU Discovery se rompe → conexiones colgadas
  - traceroute/ping para diagnóstico dejan de funcionar
  - ICMPv6 es obligatorio — bloquearlo rompe IPv6 completamente
  - Los ataques basados en ICMP son históricos y están mitigados

Consenso profesional:
  NO bloquear ICMP completamente.
  Filtrar selectivamente si es necesario.
```

### Qué ICMP permitir siempre

```bash
# IPv4 — tipos que NUNCA debes bloquear
Type 3:    Destination Unreachable  (especialmente Code 4 para PMTUD)
Type 11:   Time Exceeded            (necesario para traceroute)
Type 0/8:  Echo Reply/Request       (ping — al menos reply)

# IPv6 — OBLIGATORIO permitir
Type 1:    Destination Unreachable
Type 2:    Packet Too Big
Type 3:    Time Exceeded
Type 128/129: Echo
Type 133-136: NDP (RS, RA, NS, NA) — sin esto IPv6 no funciona
```

### Ejemplo de firewall que permite ICMP esencial

```bash
# iptables — permitir ICMP esencial
sudo iptables -A INPUT -p icmp --icmp-type echo-request -j ACCEPT
sudo iptables -A INPUT -p icmp --icmp-type echo-reply -j ACCEPT
sudo iptables -A INPUT -p icmp --icmp-type destination-unreachable -j ACCEPT
sudo iptables -A INPUT -p icmp --icmp-type time-exceeded -j ACCEPT

# ip6tables — permitir ICMPv6 (mejor permitir todo)
sudo ip6tables -A INPUT -p icmpv6 -j ACCEPT
```

### Rate limiting (compromiso)

Si te preocupa un abuso de ICMP, limita la tasa en lugar de bloquearlo:

```bash
# Permitir máximo 10 pings por segundo
sudo iptables -A INPUT -p icmp --icmp-type echo-request \
    -m limit --limit 10/sec --limit-burst 20 -j ACCEPT
sudo iptables -A INPUT -p icmp --icmp-type echo-request -j DROP
```

---

## 10. Errores comunes

### Error 1: "Ping falla, el host está caído"

```
Ping puede fallar por muchas razones que no implican que el host está caído:

  - Firewall en el destino bloquea ICMP echo
  - Firewall intermedio bloquea ICMP
  - Ruta rota (problema de routing, no del host)
  - Tu propia interfaz no tiene link o IP

Diagnóstico correcto:
  1. ¿Tengo link?           ip link show
  2. ¿Tengo IP?             ip addr show
  3. ¿Llego al gateway?     ping 192.168.1.1
  4. ¿Llego a Internet?     ping 8.8.8.8
  5. ¿Puerto TCP abierto?   ss -tlnp / curl / ssh
```

### Error 2: Bloquear todo ICMP en un firewall

```bash
# MALA IDEA — rompe PMTUD, diagnóstico, y en IPv6 toda la red
sudo iptables -A INPUT -p icmp -j DROP
sudo ip6tables -A INPUT -p icmpv6 -j DROP   # ← CATASTRÓFICO

# CORRECTO: permitir ICMP esencial, filtrar selectivamente si necesario
```

### Error 3: Interpretar * * * en traceroute como "problema"

```
traceroute -n 8.8.8.8
 1  192.168.1.1      1.2 ms
 2  10.0.0.1         5.3 ms
 3  * * *                       ← ¿problema?
 4  72.14.236.73    12.1 ms     ← no, el tráfico sigue pasando
 5  8.8.8.8         12.5 ms

Si los saltos posteriores responden, el router del salto 3 simplemente
no envía ICMP Time Exceeded (política del ISP). No es un problema.

ES un problema si:
 3  * * *
 4  * * *    ← a partir de aquí todo son asteriscos
 5  * * *    ← el tráfico no pasa de ese punto
```

### Error 4: Confundir alta latencia en un salto con un problema real

```
 1  192.168.1.1      1.0 ms
 2  10.0.0.1         5.0 ms
 3  72.14.236.73    80.0 ms    ← ¿lento?
 4  72.14.237.1      6.0 ms    ← pero el siguiente es rápido...

El salto 3 tiene 80ms de latencia para ICMP, pero el tráfico real
llega al salto 4 en 6ms. El router del salto 3 simplemente prioriza
el tráfico de datos sobre las respuestas ICMP (rate limiting interno).

ES un problema si la latencia alta se propaga:
 3  72.14.236.73    80.0 ms
 4  72.14.237.1     82.0 ms    ← alta latencia se mantiene
 5  8.8.8.8         85.0 ms    ← todo lo posterior es lento
```

### Error 5: No usar -n con ping/traceroute en diagnóstico

```bash
# LENTO: cada IP se resuelve con DNS inverso
traceroute 8.8.8.8
# Si DNS está roto, traceroute parece colgarse

# RÁPIDO: sin resolución DNS
traceroute -n 8.8.8.8

# Regla: cuando diagnosticas red, usa siempre -n
# DNS puede ser PARTE del problema que estás diagnosticando
```

---

## 11. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║                      ICMP — REFERENCIA RÁPIDA                        ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                      ║
║  TIPOS ICMPv4 CLAVE                                                  ║
║  Type 0:  Echo Reply               (respuesta a ping)                ║
║  Type 3:  Destination Unreachable   (error de destino)               ║
║    Code 0: Network Unreachable                                       ║
║    Code 1: Host Unreachable                                          ║
║    Code 3: Port Unreachable         (UDP cerrado)                    ║
║    Code 4: Fragmentation Needed     (PMTUD)                          ║
║  Type 5:  Redirect                  (ruta mejor disponible)          ║
║  Type 8:  Echo Request              (ping)                           ║
║  Type 11: Time Exceeded             (TTL=0, traceroute)              ║
║                                                                      ║
║  PING                                                                ║
║  ping -c N IP              Enviar N pings                            ║
║  ping -n IP                Sin resolución DNS                        ║
║  ping -s SIZE IP           Payload de SIZE bytes                     ║
║  ping -i SEC IP            Intervalo entre pings                     ║
║  ping -M do -s 1472 IP     Probar MTU (DF=1)                        ║
║  ping -6 IP                Ping IPv6                                 ║
║                                                                      ║
║  TRACEROUTE                                                          ║
║  traceroute -n IP          Sin DNS (más rápido)                      ║
║  traceroute -I IP          Usar ICMP en vez de UDP                   ║
║  traceroute -T -p 80 IP   Usar TCP al puerto 80                     ║
║  tracepath IP              Descubre MTU, sin root                    ║
║  mtr -n IP                 Traceroute continuo + estadísticas        ║
║                                                                      ║
║  INTERPRETAR RESULTADOS                                              ║
║  * * *  con saltos posteriores OK = router filtra ICMP (normal)      ║
║  * * *  hasta el final = ruta rota o host inalcanzable               ║
║  Latencia alta en un salto, baja después = router rate-limita ICMP   ║
║  Latencia alta que se propaga = problema real en ese salto            ║
║                                                                      ║
║  ICMPv6 (OBLIGATORIO — NO BLOQUEAR)                                 ║
║  Type 133-136: NDP (reemplaza ARP + router discovery)                ║
║  Type 128/129: Echo Request/Reply                                    ║
║  Type 2: Packet Too Big (PMTUD)                                      ║
║                                                                      ║
║  FIREWALLS                                                           ║
║  Permitir siempre: Type 0, 3, 8, 11 (IPv4)                          ║
║  Permitir siempre: todo ICMPv6 (o mínimo 1,2,3,128,129,133-136)     ║
║  Rate-limit si preocupa abuso: --limit 10/sec                        ║
║  NUNCA: bloquear todo ICMP                                           ║
║                                                                      ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 12. Ejercicios

### Ejercicio 1: Anatomía de un ping

1. Ejecuta un ping a tu gateway con verbose:
   ```bash
   ping -c 5 -n $(ip route show default | awk '{print $3}')
   ```
2. Anota para cada paquete: seq, ttl, time
3. Calcula el RTT promedio, mínimo y máximo manualmente y compáralos con la
   línea de estadísticas final
4. El TTL de la respuesta indica el sistema operativo del router:
   - TTL ~64 → Linux
   - TTL ~128 → Windows
   - TTL ~255 → Cisco/Network equipment

   ¿Qué TTL ves? ¿Cuántos saltos hay entre tú y el gateway?

5. Ahora haz ping a un host remoto:
   ```bash
   ping -c 5 -n 8.8.8.8
   ```
   Compara el TTL y el RTT con el ping al gateway.

**Pregunta de reflexión**: si el TTL de la respuesta de `8.8.8.8` es 118 y asumes
que empezó en 128, ¿cuántos routers hay en la ruta? ¿Coincide con lo que muestra
`traceroute -n 8.8.8.8`?

### Ejercicio 2: Traceroute comparativo

1. Ejecuta traceroute con tres métodos diferentes:
   ```bash
   # UDP (default)
   traceroute -n 8.8.8.8

   # ICMP
   sudo traceroute -I -n 8.8.8.8

   # TCP port 443
   sudo traceroute -T -p 443 -n 8.8.8.8
   ```
2. Compara los resultados:
   - ¿Muestran los mismos saltos?
   - ¿Algún método muestra saltos que otro no? (menos `* * *`)
   - ¿Cuál llega más lejos?

3. Prueba `mtr` para ver las estadísticas en tiempo real:
   ```bash
   mtr -n 8.8.8.8
   ```
   Déjalo correr 30 segundos y observa la columna Loss%.

**Pregunta de reflexión**: ¿por qué TCP traceroute al puerto 443 puede mostrar
más saltos que ICMP traceroute? Piensa en qué reglas de firewall son más comunes
en Internet: ¿bloquear ICMP o bloquear HTTPS?

### Ejercicio 3: ICMP y diagnóstico de red

Simula un diagnóstico de red siguiendo el método de abajo hacia arriba:

1. Verifica capa de enlace:
   ```bash
   ip link show enp0s3    # ¿state UP?
   ```
2. Verifica capa de red local:
   ```bash
   ping -c 1 -n 127.0.0.1        # loopback
   ping -c 1 -n TU_IP_LOCAL      # tu propia IP
   ping -c 1 -n TU_GATEWAY       # gateway
   ```
3. Verifica conectividad a Internet por IP:
   ```bash
   ping -c 1 -n 8.8.8.8          # DNS de Google
   ping -c 1 -n 1.1.1.1          # DNS de Cloudflare
   ```
4. Verifica DNS:
   ```bash
   ping -c 1 google.com          # ¿resuelve el nombre?
   ```
5. Si el paso 4 falla pero el 3 funciona, el problema es DNS, no red.

**Pregunta de predicción**: si `ping 8.8.8.8` funciona pero `ping google.com` falla,
¿qué mensaje de error esperas ver? ¿Es un error ICMP o un error de otra capa?

**Pregunta de reflexión**: este método de diagnóstico sigue las capas del modelo
TCP/IP de abajo hacia arriba. ¿Por qué es más eficiente que empezar probando desde
la capa de aplicación (ej: `curl http://google.com`)?
