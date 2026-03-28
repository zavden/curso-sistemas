# tcpdump — Captura y análisis de paquetes

## Índice

1. [Qué es tcpdump](#1-qué-es-tcpdump)
2. [Sintaxis básica y opciones principales](#2-sintaxis-básica-y-opciones-principales)
3. [Leer la salida de tcpdump](#3-leer-la-salida-de-tcpdump)
4. [Filtros BPF — expresiones de captura](#4-filtros-bpf--expresiones-de-captura)
5. [Filtros por host y red](#5-filtros-por-host-y-red)
6. [Filtros por puerto y protocolo](#6-filtros-por-puerto-y-protocolo)
7. [Filtros combinados](#7-filtros-combinados)
8. [Captura a archivo y lectura offline](#8-captura-a-archivo-y-lectura-offline)
9. [Opciones de verbosidad y formato](#9-opciones-de-verbosidad-y-formato)
10. [Análisis de protocolos comunes](#10-análisis-de-protocolos-comunes)
11. [Escenarios de diagnóstico](#11-escenarios-de-diagnóstico)
12. [Errores comunes](#12-errores-comunes)
13. [Cheatsheet](#13-cheatsheet)
14. [Ejercicios](#14-ejercicios)

---

## 1. Qué es tcpdump

tcpdump es el analizador de paquetes de línea de comandos más extendido en
Linux/Unix. Captura tráfico de red en tiempo real o lo guarda a archivo para
análisis posterior. Usa la librería `libpcap` para acceder a los paquetes
directamente desde la interfaz de red.

### Posición en el stack de diagnóstico

```
Problema reportado
     │
     ▼
  ss -tanp          ← ¿El servicio escucha? ¿Hay conexiones?
     │
     ▼
  tcpdump           ← ¿Los paquetes llegan? ¿Qué contienen?
     │
     ▼
  Wireshark         ← Análisis visual profundo (GUI, sobre archivos pcap)
```

`ss` te dice el estado de las conexiones. tcpdump te muestra los **paquetes
individuales** que circulan por la red — es el nivel más bajo de diagnóstico
antes de recurrir a un analizador de protocolos con GUI.

### Instalación

```bash
# Debian/Ubuntu
apt install tcpdump

# RHEL/Fedora
dnf install tcpdump

# Verificar
tcpdump --version
```

tcpdump requiere **privilegios de root** (o capability `CAP_NET_RAW`) para
capturar paquetes.

---

## 2. Sintaxis básica y opciones principales

### Formato

```
tcpdump [opciones] [expresión de filtro]
```

### Opciones fundamentales

| Opción | Significado |
|--------|------------|
| `-i <interfaz>` | Interfaz a capturar (`-i eth0`, `-i any`) |
| `-n` | No resolver nombres DNS |
| `-nn` | No resolver ni nombres ni puertos (más rápido) |
| `-c <N>` | Capturar solo N paquetes y parar |
| `-w <archivo>` | Escribir captura a archivo pcap |
| `-r <archivo>` | Leer captura desde archivo pcap |
| `-v` | Verbosidad nivel 1 (TTL, TOS, ID) |
| `-vv` | Verbosidad nivel 2 (más opciones de protocolo) |
| `-vvv` | Verbosidad nivel 3 (todo) |
| `-X` | Mostrar payload en hex y ASCII |
| `-XX` | Como -X pero incluye cabecera Ethernet |
| `-A` | Mostrar payload solo en ASCII |
| `-e` | Mostrar cabecera Ethernet (MACs) |
| `-q` | Salida resumida (quiet) |
| `-t` | No mostrar timestamp |
| `-tttt` | Timestamp completo (fecha y hora legible) |
| `-s <N>` | Snap length: capturar N bytes de cada paquete |
| `-S` | Números de secuencia absolutos (no relativos) |

### Los comandos más usados

```bash
# Capturar todo en una interfaz (ctrl-c para parar)
sudo tcpdump -i eth0

# Capturar con resolución desactivada (más rápido y legible)
sudo tcpdump -i eth0 -nn

# Capturar N paquetes
sudo tcpdump -i eth0 -nn -c 100

# Capturar en todas las interfaces
sudo tcpdump -i any -nn

# Capturar y guardar a archivo
sudo tcpdump -i eth0 -nn -w /tmp/capture.pcap

# Leer archivo de captura
tcpdump -nn -r /tmp/capture.pcap
```

---

## 3. Leer la salida de tcpdump

### Formato de una línea TCP

```
14:23:01.123456 IP 192.168.1.10.45832 > 203.0.113.5.443: Flags [S], seq 123456, win 65535, options [...], length 0
│               │  │                     │                 │          │          │           │             │
│               │  │                     │                 │          │          │           │             └─ bytes de payload
│               │  │                     │                 │          │          │           └─ opciones TCP
│               │  │                     │                 │          │          └─ tamaño de ventana
│               │  │                     │                 │          └─ número de secuencia
│               │  │                     │                 └─ flags TCP
│               │  │                     └─ destino IP.puerto
│               │  └─ origen IP.puerto
│               └─ protocolo (IP)
└─ timestamp (HH:MM:SS.microsegundos)
```

### Flags TCP

```
[S]     = SYN          (inicio de conexión)
[S.]    = SYN-ACK      (respuesta al SYN)
[.]     = ACK          (acknowledgement, el punto = ACK)
[P.]    = PSH-ACK      (push + ack, datos)
[F.]    = FIN-ACK      (cierre de conexión)
[R]     = RST          (reset, cierre abrupto)
[R.]    = RST-ACK      (reset con acknowledgement)
```

### Ejemplo: handshake TCP completo

```bash
$ sudo tcpdump -i eth0 -nn port 80 -c 3

# 1. SYN (cliente → servidor)
14:23:01.100 IP 192.168.1.10.45832 > 203.0.113.5.80: Flags [S], seq 1000, win 65535, length 0

# 2. SYN-ACK (servidor → cliente)
14:23:01.105 IP 203.0.113.5.80 > 192.168.1.10.45832: Flags [S.], seq 2000, ack 1001, win 28960, length 0

# 3. ACK (cliente → servidor)
14:23:01.105 IP 192.168.1.10.45832 > 203.0.113.5.80: Flags [.], ack 2001, win 65535, length 0
```

### Formato UDP

```
14:23:01.200 IP 192.168.1.10.39221 > 8.8.8.8.53: 12345+ A? www.example.com. (33)
│               │                    │              │      │  │                 │
│               │                    │              │      │  │                 └─ tamaño
│               │                    │              │      │  └─ dominio consultado
│               │                    │              │      └─ tipo de consulta (A)
│               │                    │              └─ ID de transacción DNS
│               │                    └─ destino (DNS server)
│               └─ origen
└─ timestamp
```

### Formato ICMP

```
14:23:01.300 IP 192.168.1.10 > 8.8.8.8: ICMP echo request, id 1234, seq 1, length 64
14:23:01.315 IP 8.8.8.8 > 192.168.1.10: ICMP echo reply, id 1234, seq 1, length 64
```

---

## 4. Filtros BPF — expresiones de captura

tcpdump usa **Berkeley Packet Filter (BPF)** para filtrar paquetes directamente
en el kernel, antes de copiarlos al espacio de usuario. Esto es crucial: sin
filtro, tcpdump captura **todo** el tráfico, lo que en una red con mucho
tráfico puede ser inmanejable.

### Estructura de los filtros

Los filtros se componen de **primitivas** combinadas con operadores lógicos:

```
Primitivas:         host, net, port, src, dst, proto, ...
Operadores:         and (&&), or (||), not (!)
Agrupación:         ( )
```

### Prioridad de operadores

```
not  >  and  >  or

# Ejemplo sin paréntesis:
not host 10.0.0.1 and port 80
# Equivale a: (not host 10.0.0.1) and (port 80)

# Con paréntesis para cambiar significado:
not (host 10.0.0.1 and port 80)
# Excluye solo el tráfico de 10.0.0.1 AL puerto 80
```

**Importante**: en la shell, los paréntesis deben escaparse o ir entre comillas:

```bash
# Escapar paréntesis
sudo tcpdump -i eth0 -nn \( host 10.0.0.1 or host 10.0.0.2 \) and port 80

# O usar comillas simples para todo el filtro
sudo tcpdump -i eth0 -nn '( host 10.0.0.1 or host 10.0.0.2 ) and port 80'
```

---

## 5. Filtros por host y red

### Por host

```bash
# Todo el tráfico de/hacia una IP
sudo tcpdump -i eth0 -nn host 192.168.1.100

# Solo tráfico DESDE una IP
sudo tcpdump -i eth0 -nn src host 192.168.1.100

# Solo tráfico HACIA una IP
sudo tcpdump -i eth0 -nn dst host 192.168.1.100

# Por nombre (tcpdump resuelve al inicio)
sudo tcpdump -i eth0 -nn host www.example.com
```

### Por red

```bash
# Todo el tráfico de/hacia una subred
sudo tcpdump -i eth0 -nn net 192.168.1.0/24

# Tráfico DESDE una subred
sudo tcpdump -i eth0 -nn src net 10.0.0.0/8

# Tráfico HACIA una subred
sudo tcpdump -i eth0 -nn dst net 172.16.0.0/12
```

### Entre dos hosts

```bash
# Tráfico entre dos IPs específicas (en cualquier dirección)
sudo tcpdump -i eth0 -nn host 192.168.1.10 and host 192.168.1.20

# Excluyendo un host (útil para filtrar tu propia sesión SSH)
sudo tcpdump -i eth0 -nn not host 192.168.1.50
```

### Filtrar por MAC (capa 2)

```bash
# Por dirección Ethernet
sudo tcpdump -i eth0 -nn ether host aa:bb:cc:dd:ee:ff

# Broadcast Ethernet
sudo tcpdump -i eth0 -nn ether broadcast
```

---

## 6. Filtros por puerto y protocolo

### Por puerto

```bash
# Todo el tráfico al/desde puerto 80
sudo tcpdump -i eth0 -nn port 80

# Solo tráfico al puerto 80 (dirección destino)
sudo tcpdump -i eth0 -nn dst port 80

# Solo tráfico desde el puerto 80 (respuestas del servidor)
sudo tcpdump -i eth0 -nn src port 80

# Rango de puertos
sudo tcpdump -i eth0 -nn portrange 8000-8100

# Múltiples puertos
sudo tcpdump -i eth0 -nn port 80 or port 443 or port 8080
```

### Por protocolo

```bash
# Solo TCP
sudo tcpdump -i eth0 -nn tcp

# Solo UDP
sudo tcpdump -i eth0 -nn udp

# Solo ICMP
sudo tcpdump -i eth0 -nn icmp

# Solo ARP
sudo tcpdump -i eth0 -nn arp

# Solo tráfico IPv6
sudo tcpdump -i eth0 -nn ip6

# Protocolo específico por número
sudo tcpdump -i eth0 -nn proto 47    # GRE
```

### Combinar protocolo y puerto

```bash
# Solo TCP al puerto 22
sudo tcpdump -i eth0 -nn tcp port 22

# Solo UDP al puerto 53
sudo tcpdump -i eth0 -nn udp port 53

# TCP al puerto 80 o 443
sudo tcpdump -i eth0 -nn 'tcp and ( port 80 or port 443 )'
```

### Filtrar por flags TCP

```bash
# Solo paquetes SYN (inicio de conexión)
sudo tcpdump -i eth0 -nn 'tcp[tcpflags] & tcp-syn != 0'

# Solo SYN sin ACK (primer paquete del handshake)
sudo tcpdump -i eth0 -nn 'tcp[tcpflags] == tcp-syn'

# Solo RST (resets)
sudo tcpdump -i eth0 -nn 'tcp[tcpflags] & tcp-rst != 0'

# Solo FIN (cierre de conexión)
sudo tcpdump -i eth0 -nn 'tcp[tcpflags] & tcp-fin != 0'

# Solo paquetes con payload (PSH)
sudo tcpdump -i eth0 -nn 'tcp[tcpflags] & tcp-push != 0'

# SYN flood detection: SYN sin respuesta
sudo tcpdump -i eth0 -nn 'tcp[tcpflags] == tcp-syn' and dst port 80
```

---

## 7. Filtros combinados

### Operadores

```bash
# AND: ambas condiciones deben cumplirse
sudo tcpdump -i eth0 -nn host 192.168.1.100 and port 80

# OR: al menos una condición
sudo tcpdump -i eth0 -nn port 80 or port 443

# NOT: excluir
sudo tcpdump -i eth0 -nn not port 22

# Combinaciones complejas con paréntesis
sudo tcpdump -i eth0 -nn 'host 192.168.1.100 and ( port 80 or port 443 )'
```

### Filtros prácticos del día a día

```bash
# Todo el tráfico HTTP/HTTPS excepto mi SSH
sudo tcpdump -i eth0 -nn 'not port 22 and ( port 80 or port 443 )'

# Tráfico DNS
sudo tcpdump -i eth0 -nn 'udp port 53 or tcp port 53'

# Tráfico DHCP
sudo tcpdump -i eth0 -nn 'udp port 67 or udp port 68'

# Tráfico a IPs externas (no RFC 1918)
sudo tcpdump -i eth0 -nn 'not ( net 10.0.0.0/8 or net 172.16.0.0/12 or net 192.168.0.0/16 )'

# Solo paquetes nuevas conexiones TCP al servidor web
sudo tcpdump -i eth0 -nn 'tcp[tcpflags] == tcp-syn and dst port 80'

# Paquetes ICMP unreachable (útil para diagnosticar MTU/routing)
sudo tcpdump -i eth0 -nn 'icmp[icmptype] == icmp-unreach'

# Tráfico entre dos redes
sudo tcpdump -i eth0 -nn 'src net 192.168.1.0/24 and dst net 10.0.0.0/24'
```

---

## 8. Captura a archivo y lectura offline

### Guardar captura

```bash
# Captura básica a archivo pcap
sudo tcpdump -i eth0 -nn -w /tmp/capture.pcap

# Con filtro (captura menos datos)
sudo tcpdump -i eth0 -nn -w /tmp/web.pcap 'port 80 or port 443'

# Limitar número de paquetes
sudo tcpdump -i eth0 -nn -w /tmp/sample.pcap -c 1000

# Limitar tamaño de captura por paquete (snap length)
sudo tcpdump -i eth0 -nn -w /tmp/headers.pcap -s 96
# 96 bytes captura cabeceras pero no payload (menos espacio, más privacidad)

# Captura completa (paquetes enteros, default en versiones modernas)
sudo tcpdump -i eth0 -nn -w /tmp/full.pcap -s 0
```

### Rotación de archivos

Para capturas prolongadas, tcpdump puede rotar archivos automáticamente:

```bash
# Rotar cada 100MB, mantener 10 archivos
sudo tcpdump -i eth0 -nn -w /tmp/capture.pcap \
    -C 100 -W 10

# Los archivos se numeran: capture.pcap0, capture.pcap1, ...

# Rotar cada 3600 segundos (1 hora), mantener 24 archivos
sudo tcpdump -i eth0 -nn -w /tmp/capture.pcap \
    -G 3600 -W 24

# Nombre con timestamp (requiere strftime en -w)
sudo tcpdump -i eth0 -nn -w /tmp/capture_%Y%m%d_%H%M%S.pcap \
    -G 3600
```

### Leer archivos de captura

```bash
# Leer archivo completo
tcpdump -nn -r /tmp/capture.pcap

# Leer con filtro (aplicar filtro post-captura)
tcpdump -nn -r /tmp/capture.pcap 'port 80'

# Leer con verbosidad
tcpdump -nn -vv -r /tmp/capture.pcap

# Leer mostrando payload ASCII
tcpdump -nn -A -r /tmp/capture.pcap 'port 80'

# Leer mostrando payload hex + ASCII
tcpdump -nn -X -r /tmp/capture.pcap

# Contar paquetes sin mostrarlos
tcpdump -nn -r /tmp/capture.pcap | wc -l

# Contar paquetes por filtro
tcpdump -nn -r /tmp/capture.pcap 'tcp[tcpflags] == tcp-syn' | wc -l
```

### Convertir para Wireshark

Los archivos `.pcap` generados por tcpdump se abren directamente en Wireshark:

```bash
# Capturar en el servidor
sudo tcpdump -i eth0 -nn -w /tmp/capture.pcap -c 5000

# Copiar al equipo local
scp server:/tmp/capture.pcap .

# Abrir en Wireshark (en tu equipo local con GUI)
wireshark capture.pcap
```

---

## 9. Opciones de verbosidad y formato

### Niveles de verbosidad

```bash
# Sin -v: línea básica
14:23:01.100 IP 192.168.1.10.45832 > 203.0.113.5.80: Flags [S], seq 1000, win 65535, length 0

# -v: añade TTL, TOS, ID, longitud total
14:23:01.100 IP (tos 0x0, ttl 64, id 54321, offset 0, flags [DF], proto TCP (6), length 60)
    192.168.1.10.45832 > 203.0.113.5.80: Flags [S], seq 1000, win 65535, options [...], length 0

# -vv: añade checksums, opciones TCP desglosadas
# -vvv: máximo detalle
```

### Formato de timestamps

```bash
# Default: HH:MM:SS.microsegundos
sudo tcpdump -i eth0 -nn -c 1
# 14:23:01.123456

# -t: sin timestamp
sudo tcpdump -i eth0 -nn -t -c 1
# IP 192.168.1.10.45832 > ...

# -tt: timestamp Unix (epoch)
sudo tcpdump -i eth0 -nn -tt -c 1
# 1711029781.123456

# -ttt: delta entre paquetes
sudo tcpdump -i eth0 -nn -ttt -c 5
# 00:00:00.000000   (primer paquete)
# 00:00:00.004523   (4.5ms después)
# 00:00:00.000089   (0.089ms después)

# -tttt: fecha y hora completas
sudo tcpdump -i eth0 -nn -tttt -c 1
# 2026-03-21 14:23:01.123456
```

`-ttt` es especialmente útil para ver latencia entre paquetes de una misma
conversación.

### Mostrar contenido del paquete

```bash
# -A: payload en ASCII (útil para HTTP, SMTP, texto plano)
sudo tcpdump -i eth0 -nn -A 'port 80' -c 5
# ...
# GET /index.html HTTP/1.1
# Host: www.example.com
# User-Agent: curl/7.81.0
# Accept: */*

# -X: payload en hex + ASCII (útil para protocolos binarios)
sudo tcpdump -i eth0 -nn -X 'port 53' -c 1
# 0x0000:  4500 003c 1234 0000 4011 abcd c0a8 010a  E..<.4..@.......
# 0x0010:  0808 0808 9945 0035 0028 abcd 1234 0100  .....E.5.(..4...

# -e: mostrar cabeceras Ethernet (MACs)
sudo tcpdump -i eth0 -nn -e -c 1
# 14:23:01.100 aa:bb:cc:dd:ee:f1 > aa:bb:cc:dd:ee:f2, ethertype IPv4 (0x0800),
#   length 74: 192.168.1.10.45832 > 203.0.113.5.80: Flags [S], ...
```

### Otras opciones útiles

```bash
# -q: modo quiet (menos detalle por línea)
sudo tcpdump -i eth0 -nn -q -c 5
# 14:23:01.100 IP 192.168.1.10.45832 > 203.0.113.5.80: tcp 0
# (sin flags, opciones ni detalles)

# -l: line-buffered (útil para pipear a grep en tiempo real)
sudo tcpdump -i eth0 -nn -l | grep "HTTP"

# -#: numerar paquetes
sudo tcpdump -i eth0 -nn -# -c 5
#     1  14:23:01.100 IP ...
#     2  14:23:01.105 IP ...
```

---

## 10. Análisis de protocolos comunes

### DNS

```bash
# Capturar tráfico DNS
sudo tcpdump -i eth0 -nn 'udp port 53' -vv

# Salida típica:
# 14:23:01.200 IP 192.168.1.10.39221 > 8.8.8.8.53:
#   12345+ [1au] A? www.example.com. (44)
#                │         │  │
#                │         │  └─ tipo de consulta: A (IPv4)
#                │         └─ dominio
#                └─ ID de transacción

# Respuesta:
# 14:23:01.220 IP 8.8.8.8.53 > 192.168.1.10.39221:
#   12345 1/0/1 A 93.184.216.34 (56)
#         │     │
#         │     └─ respuesta: un registro A
#         └─ misma transacción
```

Qué buscar:
- Queries sin respuesta → problema DNS o firewall bloqueando
- Respuestas NXDOMAIN → el dominio no existe
- Respuestas con IPs inesperadas → posible DNS hijacking
- Latencia entre query y response → DNS lento

### HTTP (texto plano)

```bash
# Ver peticiones HTTP
sudo tcpdump -i eth0 -nn -A 'tcp port 80 and tcp[tcpflags] & tcp-push != 0' | \
    grep -E "^(GET|POST|PUT|DELETE|HEAD|HTTP)"

# Captura completa con payload
sudo tcpdump -i eth0 -nn -A 'port 80' -s 0

# Salida típica:
# GET /api/status HTTP/1.1
# Host: api.example.com
# Authorization: Bearer eyJ...
#
# HTTP/1.1 200 OK
# Content-Type: application/json
# {"status": "ok"}
```

**Nota**: HTTPS (puerto 443) está cifrado. Solo verás el handshake TLS
(Client Hello / Server Hello), no el contenido. Para analizar HTTPS
necesitarías la clave privada del servidor o un proxy de intercepción.

### TLS/HTTPS (lo que se puede ver)

```bash
# Capturar handshakes TLS
sudo tcpdump -i eth0 -nn 'tcp port 443 and tcp[tcpflags] & tcp-push != 0' -A | \
    grep -a "Server Name"

# Con -vv puedes ver el SNI (Server Name Indication) en el Client Hello
sudo tcpdump -i eth0 -nn -vv 'tcp port 443' -c 20 2>/dev/null | \
    grep -i "server name"
```

### ARP

```bash
# Capturar tráfico ARP
sudo tcpdump -i eth0 -nn arp

# Salida:
# 14:23:01.400 ARP, Request who-has 192.168.1.1 tell 192.168.1.10, length 28
# 14:23:01.401 ARP, Reply 192.168.1.1 is-at aa:bb:cc:dd:ee:f1, length 28

# Detectar ARP spoofing: múltiples MACs para la misma IP
sudo tcpdump -i eth0 -nn arp | grep "is-at"
```

### ICMP

```bash
# Capturar ping
sudo tcpdump -i eth0 -nn icmp

# Salida:
# echo request → echo reply = ping exitoso
# host unreachable = no hay ruta
# port unreachable = puerto cerrado (UDP)
# time exceeded = TTL expirado (traceroute)
# need to frag = paquete muy grande, DF set (problema MTU)

# Solo paquetes "destination unreachable" (diagnóstico de routing)
sudo tcpdump -i eth0 -nn 'icmp[icmptype] == 3'

# Solo "time exceeded" (ver traceroute en acción)
sudo tcpdump -i eth0 -nn 'icmp[icmptype] == 11'
```

### DHCP

```bash
# Capturar tráfico DHCP (puertos 67 y 68)
sudo tcpdump -i eth0 -nn -vv 'udp port 67 or udp port 68'

# Verás el ciclo DORA:
# Discover: 0.0.0.0.68 > 255.255.255.255.67
# Offer:    192.168.1.1.67 > 255.255.255.255.68
# Request:  0.0.0.0.68 > 255.255.255.255.67
# ACK:      192.168.1.1.67 > 255.255.255.255.68
```

---

## 11. Escenarios de diagnóstico

### Escenario 1: "El servicio no responde"

```bash
# Paso 1: ¿Llegan los paquetes SYN?
sudo tcpdump -i eth0 -nn 'tcp[tcpflags] == tcp-syn and dst port 80' -c 10

# Si NO llegan SYN → problema de routing o firewall en el camino
# Si llegan SYN pero no hay SYN-ACK → firewall local bloquea o servicio no escucha

# Paso 2: ¿Hay respuesta SYN-ACK?
sudo tcpdump -i eth0 -nn 'port 80' -c 20
# Buscar el patrón S → S. → . (handshake completo)
# Si solo ves S, S, S → SYN sin respuesta → servicio caído o bloqueado

# Paso 3: ¿Hay RST (reset)?
sudo tcpdump -i eth0 -nn 'tcp[tcpflags] & tcp-rst != 0 and port 80' -c 5
# RST = el puerto está cerrado o el servicio rechaza la conexión
```

### Escenario 2: "DNS no resuelve"

```bash
# Capturar tráfico DNS
sudo tcpdump -i eth0 -nn 'udp port 53' -vv

# En otra terminal:
dig www.example.com

# Verificar:
# ¿Sale la query? → ¿hacia qué servidor DNS?
# ¿Llega la respuesta? → si no → firewall bloquea puerto 53
# ¿La respuesta tiene la IP correcta? → si no → DNS comprometido
# ¿Hay latencia excesiva entre query y response?
```

### Escenario 3: "Conexión lenta"

```bash
# Capturar con timestamps delta entre paquetes
sudo tcpdump -i eth0 -nn -ttt 'host 192.168.1.100 and port 80' -c 50

# Buscar gaps grandes entre paquetes:
# 00:00:00.000050  ← normal (50μs)
# 00:00:00.001200  ← normal (1.2ms)
# 00:00:02.500000  ← ANORMAL (2.5 segundos de gap)
#   ↑ retransmisión o servidor lento

# Buscar retransmisiones (mismo seq number repetido)
sudo tcpdump -i eth0 -nn -S 'host 192.168.1.100 and port 80' | \
    awk '{print $NF}' | sort | uniq -d
```

### Escenario 4: verificar NAT

```bash
# En la interfaz interna: ver paquete con IPs privadas
sudo tcpdump -i eth0 -nn 'port 80' -c 5
# src=192.168.1.10 dst=203.0.113.5

# En la interfaz externa: ver paquete con IP pública (post-NAT)
sudo tcpdump -i eth1 -nn 'port 80' -c 5
# src=198.51.100.1 dst=203.0.113.5
#     ▲ IP reescrita por SNAT
```

### Escenario 5: capturar sin interferir con SSH

```bash
# Si estás conectado por SSH y capturas en la misma interfaz,
# tcpdump captura su propio tráfico SSH → output infinito

# Excluir tu sesión SSH
sudo tcpdump -i eth0 -nn 'not port 22'

# O excluir tu IP
sudo tcpdump -i eth0 -nn 'not host 192.168.1.50'

# O ser más específico
sudo tcpdump -i eth0 -nn 'not ( src host 192.168.1.50 and dst port 22 )'
```

### Escenario 6: detectar tráfico no esperado

```bash
# ¿Hay tráfico a puertos no autorizados?
sudo tcpdump -i eth0 -nn 'dst port not 22 and dst port not 80 and dst port not 443' \
    -c 100 -q

# ¿Hay tráfico saliente inesperado? (posible malware)
sudo tcpdump -i eth0 -nn 'src net 192.168.1.0/24 and not dst net 192.168.1.0/24' \
    -c 100 -q | awk '{print $5}' | cut -d. -f1-4 | sort | uniq -c | sort -rn
```

---

## 12. Errores comunes

### Error 1: capturar sin filtro en interfaz de alto tráfico

```bash
# ✗ Capturar TODO en un servidor de producción
sudo tcpdump -i eth0
# Miles de líneas por segundo, imposible leer, consume CPU

# ✓ Siempre usar filtro
sudo tcpdump -i eth0 -nn 'port 80 and host 192.168.1.100' -c 50

# ✓ O guardar a archivo y analizar después
sudo tcpdump -i eth0 -nn -w /tmp/capture.pcap -c 10000
```

### Error 2: olvidar -nn y ver resolución DNS lenta

```bash
# ✗ Sin -n: cada IP se resuelve con DNS reverso
sudo tcpdump -i eth0 port 80
# server1.example.com.http > client2.example.com.45832
# (lento, y la propia resolución DNS aparece en la captura)

# ✓ Usar -nn (no resolver nombres NI puertos)
sudo tcpdump -i eth0 -nn port 80
# 203.0.113.5.80 > 192.168.1.10.45832
```

### Error 3: capturar paquetes truncados

```bash
# ✗ Snap length insuficiente (versiones antiguas default = 68 bytes)
sudo tcpdump -i eth0 -nn -s 68 -w capture.pcap
# Solo cabeceras, payload truncado

# ✓ Capturar paquete completo
sudo tcpdump -i eth0 -nn -s 0 -w capture.pcap
# En versiones modernas -s 0 ya es el default (262144 bytes)
```

### Error 4: no incluir tráfico propio al diagnosticar

```bash
# ✗ "No hay tráfico HTTP" — pero excluiste tu propia IP
sudo tcpdump -i eth0 -nn 'not host 192.168.1.50 and port 80'
# No ves TUS propias peticiones de prueba

# ✓ Si estás probando tú mismo, no excluyas tu IP del filtro
# Excluye solo SSH para no ver la sesión de gestión
sudo tcpdump -i eth0 -nn 'not port 22 and port 80'
```

### Error 5: no escapar paréntesis en la shell

```bash
# ✗ Shell interpreta los paréntesis
sudo tcpdump -i eth0 -nn (host 10.0.0.1 or host 10.0.0.2) and port 80
# bash: syntax error near unexpected token '('

# ✓ Escapar o comillas
sudo tcpdump -i eth0 -nn '( host 10.0.0.1 or host 10.0.0.2 ) and port 80'
```

---

## 13. Cheatsheet

```bash
# ╔══════════════════════════════════════════════════════════════════╗
# ║                  TCPDUMP — CHEATSHEET                          ║
# ╠══════════════════════════════════════════════════════════════════╣
# ║                                                                ║
# ║  CAPTURA BÁSICA:                                              ║
# ║  sudo tcpdump -i eth0 -nn                  # todo en eth0     ║
# ║  sudo tcpdump -i any -nn                   # todas interfaces ║
# ║  sudo tcpdump -i eth0 -nn -c 100          # solo 100 paquetes ║
# ║  sudo tcpdump -i eth0 -nn -w file.pcap    # guardar a archivo ║
# ║  tcpdump -nn -r file.pcap                 # leer archivo      ║
# ║                                                                ║
# ║  FILTROS POR HOST/RED:                                        ║
# ║  host 192.168.1.100                        # de/hacia IP      ║
# ║  src host 10.0.0.1                         # solo desde       ║
# ║  dst host 10.0.0.1                         # solo hacia       ║
# ║  net 192.168.1.0/24                        # subred           ║
# ║                                                                ║
# ║  FILTROS POR PUERTO:                                          ║
# ║  port 80                                   # de/hacia pto 80  ║
# ║  dst port 443                              # hacia pto 443    ║
# ║  portrange 8000-8100                       # rango            ║
# ║                                                                ║
# ║  FILTROS POR PROTOCOLO:                                       ║
# ║  tcp / udp / icmp / arp                                       ║
# ║  'tcp[tcpflags] == tcp-syn'                # solo SYN         ║
# ║  'tcp[tcpflags] & tcp-rst != 0'            # con RST          ║
# ║                                                                ║
# ║  COMBINADOS:                                                  ║
# ║  'host 10.0.0.1 and port 80'              # AND              ║
# ║  'port 80 or port 443'                    # OR               ║
# ║  'not port 22'                             # NOT              ║
# ║  '( host A or host B ) and port 80'       # paréntesis       ║
# ║                                                                ║
# ║  CONTENIDO:                                                   ║
# ║  -A                        # payload ASCII                    ║
# ║  -X                        # payload hex + ASCII              ║
# ║  -e                        # cabecera Ethernet (MACs)         ║
# ║                                                                ║
# ║  FORMATO:                                                     ║
# ║  -v / -vv / -vvv           # verbosidad creciente             ║
# ║  -q                        # modo resumido                    ║
# ║  -ttt                      # delta entre paquetes             ║
# ║  -tttt                     # fecha+hora completa              ║
# ║  -#                        # numerar paquetes                 ║
# ║  -l                        # line-buffered (para pipe)        ║
# ║                                                                ║
# ║  ROTACIÓN:                                                    ║
# ║  -C 100 -W 10              # rotar cada 100MB, 10 archivos   ║
# ║  -G 3600 -W 24             # rotar cada hora, 24 archivos    ║
# ║                                                                ║
# ║  TRUCO SSH:                                                   ║
# ║  'not port 22'             # excluir tu sesión SSH            ║
# ║                                                                ║
# ╚══════════════════════════════════════════════════════════════════╝
```

---

## 14. Ejercicios

### Ejercicio 1: captura y análisis de handshake TCP

**Contexto**: necesitas verificar que un servidor web completa el handshake
TCP correctamente.

**Tareas**:

1. Inicia una captura filtrada por puerto 80 en la interfaz de loopback
2. Desde otra terminal, ejecuta `curl http://localhost/`
3. Identifica en la captura:
   - Los tres paquetes del handshake (SYN, SYN-ACK, ACK)
   - Los paquetes de datos (GET request, HTTP response)
   - El cierre de conexión (FIN/FIN-ACK o RST)
4. Repite la captura con `-ttt` y mide la latencia entre SYN y SYN-ACK
5. Guarda la captura a un archivo pcap

**Pistas**:
- `-i lo` para loopback
- El handshake en loopback debería tener latencia < 1ms
- Usa `-c 20` para limitar y que la captura termine sola
- `curl` genera una petición HTTP simple seguida de cierre

> **Pregunta de reflexión**: en loopback la latencia es prácticamente cero.
> Si capturas el mismo tráfico en una interfaz real contra un servidor remoto,
> ¿qué latencia sería normal para un servidor en el mismo data center? ¿Y
> para un servidor en otro continente? ¿Cómo afecta esto al rendimiento
> percibido de una página web?

---

### Ejercicio 2: diagnóstico DNS con tcpdump

**Contexto**: un servidor reporta que "Internet está lento" — sospechas que
es un problema de resolución DNS.

**Tareas**:

1. Captura tráfico DNS (UDP/53) con verbosidad alta (`-vv`)
2. Desde otra terminal, resuelve varios dominios con `dig`
3. Analiza la captura:
   - ¿A qué servidor DNS se envían las queries?
   - ¿Cuánto tarda cada respuesta? (usa `-ttt`)
   - ¿Hay queries sin respuesta?
   - ¿Hay queries TCP (indica respuestas truncadas)?
4. Simula un problema: configura un DNS inexistente (ej. 192.0.2.1) y
   captura los timeouts
5. Compara el comportamiento con DNS funcional vs caído

**Pistas**:
- `dig @8.8.8.8 www.example.com` fuerza un servidor DNS específico
- DNS queries sin respuesta indican firewall o servidor caído
- `-ttt` muestra cuánto espera entre el query y la respuesta
- El timeout default de `dig` es ~5 segundos

> **Pregunta de reflexión**: ¿por qué la resolución DNS lenta afecta tanto
> al rendimiento percibido? Una página web moderna puede necesitar resolver
> 10-20 dominios diferentes (CDN, analytics, APIs). Si cada resolución tarda
> 2 segundos en vez de 20ms, ¿cuánto tiempo extra se añade a la carga de
> la página?

---

### Ejercicio 3: detectar problemas de red con flags TCP

**Contexto**: un servicio web tiene conexiones que "se cuelgan" intermitentemente.

**Tareas**:

1. Captura tráfico al puerto del servicio durante 2 minutos con rotación
2. Analiza el archivo capturado buscando:
   - Paquetes RST (resets inesperados)
   - Retransmisiones (mismo seq number repetido)
   - Paquetes SYN sin SYN-ACK (conexiones que no completan)
   - Gaps de tiempo anómalos entre paquetes (usa `-ttt`)
3. Cuenta las retransmisiones vs paquetes totales para estimar % de pérdida
4. Identifica si el problema es:
   - Pérdida de paquetes (retransmisiones)
   - Servidor sobrecargado (SYN sin SYN-ACK)
   - Firewall intermitente (RST de una IP de firewall)
   - Timeout de la aplicación (gaps largos + RST)

**Pistas**:
- Retransmisiones: buscar `seq` repetidos con `-S` (secuencia absoluta)
- RST desde firewall: el RST viene de una IP intermedia, no del servidor
- Timeout aplicación: muchos segundos sin actividad, luego RST
- Guarda a pcap y analiza con filtros diferentes en cada pasada

> **Pregunta de reflexión**: tcpdump captura en un punto específico de la
> ruta del paquete. Si capturas en el servidor y ves retransmisiones, ¿el
> problema está entre el cliente y el servidor, o podría estar dentro del
> propio servidor (buffer lleno, CPU saturada)? ¿Cómo distinguirías un
> problema de red de un problema de aplicación usando solo tcpdump?

---

> **Siguiente tema**: T03 — traceroute y mtr (diagnóstico de rutas, interpretación de resultados)
