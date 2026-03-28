# Las 4 Capas del Modelo TCP/IP

## Índice

1. [Por qué necesitas un modelo de red](#1-por-qué-necesitas-un-modelo-de-red)
2. [Las 4 capas del modelo TCP/IP](#2-las-4-capas-del-modelo-tcpip)
3. [Capa 1: Enlace (Link)](#3-capa-1-enlace-link)
4. [Capa 2: Red (Internet)](#4-capa-2-red-internet)
5. [Capa 3: Transporte (Transport)](#5-capa-3-transporte-transport)
6. [Capa 4: Aplicación (Application)](#6-capa-4-aplicación-application)
7. [Flujo completo de una petición](#7-flujo-completo-de-una-petición)
8. [Comparación con el modelo OSI](#8-comparación-con-el-modelo-osi)
9. [Dónde vive cada capa en Linux](#9-dónde-vive-cada-capa-en-linux)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Por qué necesitas un modelo de red

Cuando escribes `curl https://example.com`, ocurren decenas de operaciones: tu
aplicación forma una petición HTTP, esa petición se divide en segmentos, cada
segmento se envuelve en un paquete con dirección IP, cada paquete se encapsula en
un frame Ethernet con dirección MAC, y ese frame se convierte en señales eléctricas
u ópticas en el cable.

Sin un modelo que organice estas operaciones en capas, sería imposible:
- Que un navegador funcione igual sobre Wi-Fi, Ethernet o fibra óptica
- Que TCP funcione igual con IPv4 o IPv6
- Que HTTP funcione igual sobre TCP o QUIC
- Diagnosticar problemas (¿falla el cable? ¿la ruta? ¿el servicio?)

El modelo TCP/IP organiza la comunicación en red en **4 capas independientes**, donde
cada capa tiene una responsabilidad clara y se comunica con las capas adyacentes a
través de interfaces definidas.

---

## 2. Las 4 capas del modelo TCP/IP

```
┌──────────────────────────────────────┐
│  Capa 4: APLICACIÓN                  │  HTTP, DNS, SSH, SMTP, FTP
│  (lo que quieres comunicar)          │
├──────────────────────────────────────┤
│  Capa 3: TRANSPORTE                  │  TCP, UDP
│  (cómo garantizar la entrega)        │
├──────────────────────────────────────┤
│  Capa 2: RED (Internet)              │  IP (IPv4, IPv6), ICMP
│  (cómo llegar al destino)            │
├──────────────────────────────────────┤
│  Capa 1: ENLACE (Link)               │  Ethernet, Wi-Fi, PPP
│  (cómo transmitir bits al vecino)    │
└──────────────────────────────────────┘
```

### Principio fundamental: cada capa solo habla con sus vecinas

```
App A (máquina 1)                      App B (máquina 2)
┌──────────┐                           ┌──────────┐
│ Aplicación│◄─── protocolo app ───────►│ Aplicación│
├──────────┤                           ├──────────┤
│ Transporte│◄─── protocolo transp ────►│ Transporte│
├──────────┤                           ├──────────┤
│   Red     │◄─── protocolo red ───────►│   Red     │
├──────────┤                           ├──────────┤
│  Enlace   │◄─── protocolo enlace ────►│  Enlace   │
└────┬─────┘                           └────┬─────┘
     │             Cable / Wi-Fi             │
     └───────────────────────────────────────┘
```

Cada capa cree que habla directamente con su par en el otro extremo. En realidad,
los datos bajan por las capas en el emisor, viajan por el medio físico, y suben
por las capas en el receptor.

---

## 3. Capa 1: Enlace (Link)

### Responsabilidad

Transmitir datos entre dos dispositivos **directamente conectados** (vecinos en la
misma red física). No sabe nada de destinos remotos — solo se ocupa del siguiente
salto.

### Unidad de datos: Frame

```
┌──────────────────────────────────────────────────┐
│                   FRAME                           │
├──────────┬──────────┬────────────────┬───────────┤
│ Header   │ Header   │  Payload       │  Trailer  │
│ destino  │ origen   │  (paquete IP   │  (CRC/    │
│ (MAC dst)│ (MAC src)│   completo)    │   FCS)    │
└──────────┴──────────┴────────────────┴───────────┘
```

### Conceptos clave

| Concepto | Descripción |
|---|---|
| **MAC address** | Identificador único de 48 bits de la interfaz de red (ej: `aa:bb:cc:dd:ee:ff`). Grabada en hardware |
| **Ethernet** | Protocolo de enlace más común en redes cableadas (IEEE 802.3) |
| **Wi-Fi** | Protocolo de enlace inalámbrico (IEEE 802.11) |
| **ARP** | Protocolo para descubrir qué MAC tiene una IP dentro de la red local |
| **MTU** | Maximum Transmission Unit — tamaño máximo del payload del frame (1500 bytes en Ethernet estándar) |
| **Switch** | Dispositivo que opera en esta capa — reenvía frames basándose en direcciones MAC |

### Alcance

```
[PC A] ──── switch ──── [PC B]     ← Capa de enlace: A puede hablar con B
                │
              [PC C]

La capa de enlace funciona dentro de una red local (LAN).
Para llegar a otra red necesitas la capa de red (IP + router).
```

### Ver tu MAC en Linux

```bash
ip link show
# 2: enp0s3: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 ...
#     link/ether 08:00:27:a1:b2:c3 brd ff:ff:ff:ff:ff:ff
#                ─────────────────
#                Tu dirección MAC
```

---

## 4. Capa 2: Red (Internet)

### Responsabilidad

Llevar datos desde el origen hasta el destino **a través de múltiples redes**. Determina
la ruta (routing) y direcciona los paquetes con direcciones IP.

### Unidad de datos: Paquete (Packet)

```
┌──────────────────────────────────────────────┐
│                  PAQUETE IP                   │
├──────────┬──────────┬────────────────────────┤
│ IP Header│          │  Payload               │
│ (src IP, │ Options  │  (segmento TCP/UDP     │
│  dst IP, │ (raro)   │   completo)            │
│  TTL,    │          │                        │
│  proto)  │          │                        │
└──────────┴──────────┴────────────────────────┘
```

### Conceptos clave

| Concepto | Descripción |
|---|---|
| **Dirección IP** | Identificador lógico del host en la red (ej: `192.168.1.10`, `2001:db8::1`) |
| **IPv4** | Direcciones de 32 bits (4 octetos). Espacio agotado |
| **IPv6** | Direcciones de 128 bits. Solución al agotamiento de IPv4 |
| **Routing** | Proceso de decidir por dónde enviar cada paquete hacia su destino |
| **TTL** | Time To Live — contador que decrementa en cada router. Evita loops infinitos |
| **ICMP** | Protocolo de control (ping, traceroute, mensajes de error) |
| **Router** | Dispositivo que opera en esta capa — reenvía paquetes entre redes distintas |

### Alcance

```
Red A (192.168.1.0/24)       Red B (10.0.0.0/24)
[PC A] ── switch ── [Router] ── switch ── [PC B]
192.168.1.10          │ │        10.0.0.20
                      │ │
                   Internet
                      │
                  [Servidor]
                  93.184.216.34

La capa de red permite que PC A hable con Servidor
a través de múltiples redes y routers intermedios.
```

### Diferencia clave con la capa de enlace

```
Capa de enlace:  "entregar el frame al VECINO" (MAC → MAC)
Capa de red:     "entregar el paquete al DESTINO FINAL" (IP → IP)

Analogía:
  Enlace = entregar una carta al cartero de tu zona
  Red    = la dirección postal en el sobre que indica el destino final
```

### Ver tu IP en Linux

```bash
ip addr show
# 2: enp0s3: <BROADCAST,MULTICAST,UP,LOWER_UP>
#     inet 192.168.1.10/24 brd 192.168.1.255 scope global enp0s3
#          ──────────────
#          Tu dirección IPv4

ip route show
# default via 192.168.1.1 dev enp0s3
# 192.168.1.0/24 dev enp0s3 proto kernel scope link src 192.168.1.10
```

---

## 5. Capa 3: Transporte (Transport)

### Responsabilidad

Garantizar (o no) la entrega correcta de datos entre **procesos** en distintas máquinas.
La capa de red lleva paquetes entre hosts; la capa de transporte distingue entre
múltiples aplicaciones en un mismo host usando **puertos**.

### Unidad de datos

- **TCP**: Segmento
- **UDP**: Datagrama

```
Segmento TCP:
┌──────────────────────────────────────────────┐
│              SEGMENTO TCP                     │
├──────────┬──────────┬────────────────────────┤
│ TCP Hdr  │          │  Payload               │
│ (src port│ Flags    │  (datos de la          │
│  dst port│ (SYN,ACK │   aplicación)          │
│  seq num,│  FIN,RST)│                        │
│  ack num)│          │                        │
└──────────┴──────────┴────────────────────────┘
```

### Los dos protocolos de transporte

```
TCP (Transmission Control Protocol):
  ✔ Conexión establecida (three-way handshake)
  ✔ Entrega garantizada y ordenada
  ✔ Control de flujo y congestión
  ✔ Retransmisión de paquetes perdidos
  ✗ Más overhead, mayor latencia
  → Usado por: HTTP, SSH, SMTP, FTP, bases de datos

UDP (User Datagram Protocol):
  ✔ Sin conexión previa (fire and forget)
  ✔ Menor overhead, menor latencia
  ✗ Sin garantía de entrega
  ✗ Sin garantía de orden
  ✗ Sin retransmisión
  → Usado por: DNS, DHCP, streaming, VoIP, gaming, QUIC
```

### Puertos

```
Servidor (93.184.216.34):
┌────────────────────────────┐
│  Puerto 22  → sshd         │
│  Puerto 80  → nginx (HTTP) │
│  Puerto 443 → nginx (HTTPS)│
│  Puerto 53  → named (DNS)  │
└────────────────────────────┘

La IP identifica la MÁQUINA.
El puerto identifica el PROCESO dentro de esa máquina.

IP + Puerto = Socket (ej: 93.184.216.34:443)
```

### Ver conexiones de transporte en Linux

```bash
ss -tlnp
# State   Recv-Q  Send-Q  Local Address:Port   Peer Address:Port  Process
# LISTEN  0       128     0.0.0.0:22            0.0.0.0:*          sshd
# LISTEN  0       511     0.0.0.0:80            0.0.0.0:*          nginx
```

---

## 6. Capa 4: Aplicación (Application)

### Responsabilidad

Definir el **protocolo de comunicación entre aplicaciones**. Es lo que el usuario
o programador ve directamente. Cada protocolo de aplicación define su propio formato
de mensajes y reglas de interacción.

### Protocolos comunes

| Protocolo | Puerto | Transporte | Función |
|---|---|---|---|
| **HTTP/HTTPS** | 80/443 | TCP | Web |
| **SSH** | 22 | TCP | Acceso remoto seguro |
| **DNS** | 53 | UDP (y TCP) | Resolución de nombres |
| **DHCP** | 67-68 | UDP | Asignación automática de IP |
| **SMTP** | 25/587 | TCP | Envío de email |
| **FTP** | 20-21 | TCP | Transferencia de archivos |
| **NFS** | 2049 | TCP | Filesystem de red |
| **SMB** | 445 | TCP | Compartir archivos (Windows/Samba) |

### Ejemplo: petición HTTP

```
GET /index.html HTTP/1.1        ← Línea de petición (método, ruta, versión)
Host: example.com               ← Headers
User-Agent: curl/7.88.1
Accept: */*
                                ← Línea vacía (fin de headers)
                                ← Body (vacío en GET)
```

Esta es la capa que tú controlas como desarrollador o administrador. Las capas
inferiores son transparentes — te las proporcionan el kernel y el hardware.

---

## 7. Flujo completo de una petición

Cuando ejecutas `curl http://example.com/page`:

```
EMISOR (tu máquina)                    RECEPTOR (example.com)

Capa 4 - Aplicación:                  Capa 4 - Aplicación:
"GET /page HTTP/1.1\r\n..."            Nginx lee la petición HTTP
         │                                      ▲
         ▼                                      │
Capa 3 - Transporte:                  Capa 3 - Transporte:
TCP segmenta los datos                 TCP reensambla los segmentos
src port: 54321                        dst port: 80
dst port: 80                           Verifica orden y completitud
         │                                      ▲
         ▼                                      │
Capa 2 - Red:                         Capa 2 - Red:
IP empaqueta con direcciones           IP verifica destino correcto
src: 192.168.1.10                      dst: 93.184.216.34
dst: 93.184.216.34                     TTL verificado
         │                                      ▲
         ▼                                      │
Capa 1 - Enlace:                      Capa 1 - Enlace:
Ethernet frame con MACs                Ethernet extrae el paquete
src MAC: aa:bb:cc:dd:ee:ff             Verifica CRC
dst MAC: router's MAC                  dst MAC: server's MAC
         │                                      ▲
         ▼                                      │
    ═══ Cable / Wi-Fi / Internet ══════════════╝
```

### Lo que cambia y lo que no cambia en cada salto

```
Tu PC ──── Router A ──── Router B ──── Servidor

En cada salto (router):
  IP origen:    NO cambia (192.168.1.10)  ← se preserva extremo a extremo*
  IP destino:   NO cambia (93.184.216.34) ← se preserva extremo a extremo*
  MAC origen:   SÍ cambia (MAC del router que envía)
  MAC destino:  SÍ cambia (MAC del siguiente router/destino)
  TTL:          Decrementa en 1

* Excepto con NAT, que veremos en el capítulo de firewall
```

Este concepto es fundamental: las **direcciones MAC son locales** (cambian en cada
salto) mientras que las **direcciones IP son globales** (se mantienen de extremo a
extremo).

---

## 8. Comparación con el modelo OSI

El modelo OSI tiene 7 capas. TCP/IP tiene 4. No son equivalentes exactos, pero se
mapean así:

```
         OSI (7 capas)                    TCP/IP (4 capas)
┌─────────────────────────┐
│  7. Aplicación           │
├─────────────────────────┤         ┌─────────────────────┐
│  6. Presentación         │  ──►   │  4. Aplicación       │
├─────────────────────────┤         └─────────────────────┘
│  5. Sesión               │
├─────────────────────────┤         ┌─────────────────────┐
│  4. Transporte           │  ──►   │  3. Transporte       │
├─────────────────────────┤         └─────────────────────┘
│  3. Red                  │  ──►   ┌─────────────────────┐
├─────────────────────────┤         │  2. Red (Internet)   │
                                    └─────────────────────┘
│  2. Enlace de datos      │  ──►   ┌─────────────────────┐
├─────────────────────────┤         │  1. Enlace (Link)    │
│  1. Física               │        └─────────────────────┘
└─────────────────────────┘
```

### ¿Por qué existen dos modelos?

- **OSI**: modelo teórico creado por ISO. Se enseña en academias. Las capas 5, 6 y 7
  están separadas conceptualmente pero en la práctica casi nunca se implementan por
  separado.

- **TCP/IP**: modelo práctico basado en los protocolos reales de Internet. Combina las
  capas 5-7 de OSI en una sola capa de Aplicación porque así funcionan los protocolos
  reales.

### Cuándo usar cada uno

- **Para estudiar/certificaciones**: ambos. LPIC y RHCSA mencionan OSI. Muchas preguntas
  referencian "capa 3" o "capa 7" usando numeración OSI.
- **Para trabajar**: TCP/IP. Es lo que implementa Linux y lo que configuras realmente.
- **En conversaciones técnicas**: la gente mezcla ambos. "Un firewall de capa 7" usa
  numeración OSI. "El stack TCP/IP" usa el modelo de 4 capas. Necesitas entender ambos
  para comunicarte.

### Referencia rápida de capas OSI en conversaciones

Cuando alguien dice "capa N", casi siempre usa numeración OSI:

```
"Switch de capa 2"       → opera con MACs (enlace)
"Router de capa 3"       → opera con IPs (red)
"Firewall de capa 4"     → filtra por puertos (transporte)
"Load balancer de capa 7"→ inspecciona HTTP/contenido (aplicación)
```

---

## 9. Dónde vive cada capa en Linux

```
┌─────────────────────────────────────────────────────────┐
│  Espacio de usuario (userspace)                          │
│                                                          │
│  Capa 4 - Aplicación:                                    │
│    curl, nginx, sshd, named, firefox                     │
│    Tus programas en C/Rust (socket API)                   │
│                                                          │
├──────────────────────── syscall boundary ────────────────┤
│  Espacio de kernel (kernelspace)                         │
│                                                          │
│  Capa 3 - Transporte:                                    │
│    TCP/UDP implementados en el kernel                     │
│    /proc/sys/net/ipv4/tcp_*                               │
│                                                          │
│  Capa 2 - Red:                                           │
│    IP routing, Netfilter (iptables/nftables)              │
│    /proc/sys/net/ipv4/ip_forward                          │
│                                                          │
│  Capa 1 - Enlace:                                        │
│    Drivers de red (e1000, iwlwifi, virtio-net)            │
│    /sys/class/net/                                        │
│                                                          │
├──────────────────────────────────────────────────────────┤
│  Hardware: NIC (tarjeta de red)                           │
└──────────────────────────────────────────────────────────┘
```

### Herramientas de diagnóstico por capa

```
Capa 1 - Enlace:      ip link, ethtool, arp, arping
Capa 2 - Red:         ip addr, ip route, ping, traceroute
Capa 3 - Transporte:  ss, netstat, tcpdump (filtro por puerto)
Capa 4 - Aplicación:  curl, dig, nslookup, openssl s_client
```

Cuando diagnosticas un problema de red, empiezas por la capa más baja y subes:

```
1. ¿Tiene link la interfaz?              → ip link show
2. ¿Tiene IP?                            → ip addr show
3. ¿Llega al gateway?                    → ping 192.168.1.1
4. ¿Resuelve DNS?                        → dig example.com
5. ¿Llega al servidor remoto?            → ping 93.184.216.34
6. ¿El puerto está abierto?              → ss -tlnp / nmap
7. ¿La aplicación responde?              → curl http://example.com
```

---

## 10. Errores comunes

### Error 1: Confundir IP con MAC

```
"Mi servidor tiene la MAC 192.168.1.10"  ← INCORRECTO

MAC = dirección física (hardware), 48 bits, formato aa:bb:cc:dd:ee:ff
IP  = dirección lógica (configuración), 32/128 bits, formato 192.168.1.10
```

La MAC identifica la tarjeta de red. La IP identifica al host en la red.
Un host puede cambiar de IP sin cambiar de hardware. Una NIC siempre tiene
la misma MAC (salvo spoofing).

### Error 2: Pensar que TCP/IP son un solo protocolo

```
"TCP/IP" es el NOMBRE DEL MODELO, no un solo protocolo.

TCP = protocolo de transporte (capa 3)
IP  = protocolo de red (capa 2)

Son dos protocolos en capas distintas que se usan juntos frecuentemente,
pero no siempre: UDP también usa IP, y TCP podría funcionar sobre otros
protocolos de red (en teoría).
```

### Error 3: Numerar las capas TCP/IP con números OSI

```
"HTTP está en la capa 7 del modelo TCP/IP"  ← INCORRECTO

En TCP/IP, HTTP está en la capa 4 (Aplicación).
En OSI, HTTP está en la capa 7 (Aplicación).

Son modelos distintos con numeración distinta.
Si dices "capa 7", se asume OSI.
Si dices "capa de aplicación", aplica a ambos.
```

### Error 4: Creer que el modelo de capas es rígido en la práctica

Algunos protocolos no encajan limpiamente:
- **ARP**: ¿enlace o red? Opera entre ambas capas
- **ICMP**: es de capa de red pero lo usa ping (que parece aplicación)
- **TLS**: ¿transporte o aplicación? Se inserta entre ambas

El modelo es una herramienta mental para organizar conceptos, no una ley física.

### Error 5: Ignorar las capas bajas al diagnosticar

```
"curl no conecta, debe ser un problema del servidor"

Diagnóstico correcto (de abajo hacia arriba):
1. ip link show         → ¿link UP?
2. ip addr show         → ¿tiene IP?
3. ping gateway         → ¿llega al router?
4. ping 8.8.8.8         → ¿sale a Internet?
5. dig example.com      → ¿resuelve DNS?
6. curl http://...      → ahora sí, ¿responde la app?

El 80% de "problemas del servidor" son problemas de red local.
```

---

## 11. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║              MODELO TCP/IP — REFERENCIA RÁPIDA                       ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                      ║
║  CAPA 4: APLICACIÓN                                                  ║
║  Protocolos: HTTP, DNS, SSH, SMTP, FTP, NFS, SMB                     ║
║  Unidad: Mensaje / Dato                                              ║
║  Herramientas: curl, dig, openssl                                    ║
║                                                                      ║
║  CAPA 3: TRANSPORTE                                                  ║
║  Protocolos: TCP (fiable), UDP (rápido)                              ║
║  Unidad: Segmento (TCP) / Datagrama (UDP)                            ║
║  Identifica: Procesos (por puerto)                                   ║
║  Herramientas: ss, netstat, tcpdump                                  ║
║                                                                      ║
║  CAPA 2: RED (Internet)                                              ║
║  Protocolos: IPv4, IPv6, ICMP                                        ║
║  Unidad: Paquete (Packet)                                            ║
║  Identifica: Hosts (por dirección IP)                                ║
║  Herramientas: ip addr, ip route, ping, traceroute                   ║
║                                                                      ║
║  CAPA 1: ENLACE (Link)                                               ║
║  Protocolos: Ethernet, Wi-Fi, PPP                                    ║
║  Unidad: Frame                                                       ║
║  Identifica: Interfaces (por dirección MAC)                          ║
║  Herramientas: ip link, ethtool, arp                                 ║
║                                                                      ║
║  DIAGNÓSTICO (de abajo hacia arriba)                                 ║
║  1. ip link show          ¿link activo?                              ║
║  2. ip addr show          ¿tiene IP?                                 ║
║  3. ping gateway          ¿llega al router?                          ║
║  4. ping 8.8.8.8          ¿sale a Internet?                          ║
║  5. dig example.com       ¿resuelve DNS?                             ║
║  6. curl http://...       ¿responde la app?                          ║
║                                                                      ║
║  OSI vs TCP/IP (referencia rápida)                                   ║
║  "Capa 2" (OSI) = Enlace       = Capa 1 TCP/IP                      ║
║  "Capa 3" (OSI) = Red          = Capa 2 TCP/IP                      ║
║  "Capa 4" (OSI) = Transporte   = Capa 3 TCP/IP                      ║
║  "Capa 7" (OSI) = Aplicación   = Capa 4 TCP/IP                      ║
║                                                                      ║
║  RECUERDA                                                            ║
║  MAC = local (cambia en cada salto)                                  ║
║  IP  = global (se preserva extremo a extremo, salvo NAT)            ║
║  Puerto = identifica el proceso dentro del host                      ║
║                                                                      ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 12. Ejercicios

### Ejercicio 1: Identificar las capas

Examina tu propia máquina y extrae información de cada capa:

1. **Capa de enlace**: ejecuta `ip link show` e identifica:
   - El nombre de tu interfaz de red principal
   - Su dirección MAC
   - El MTU configurado
   - El estado (UP/DOWN)

2. **Capa de red**: ejecuta `ip addr show` e `ip route show` e identifica:
   - Tu dirección IPv4
   - Tu máscara de red (notación CIDR)
   - Tu gateway por defecto
   - Si tienes dirección IPv6 (link-local al menos)

3. **Capa de transporte**: ejecuta `ss -tlnp` e identifica:
   - Qué servicios están escuchando
   - En qué puertos
   - Si escuchan en `0.0.0.0` (todas las interfaces) o en una IP específica

4. **Capa de aplicación**: ejecuta `curl -v http://example.com` e identifica en la
   salida detallada:
   - La línea de petición HTTP
   - Los headers enviados
   - El código de respuesta
   - Los headers de respuesta

**Pregunta de reflexión**: cuando ejecutaste `curl`, ¿en qué momento actuó cada capa?
¿Podrías haber llegado al paso 4 si alguno de los pasos 1-3 hubiera fallado?

### Ejercicio 2: Trazar el recorrido de un paquete

1. Ejecuta `traceroute -n 8.8.8.8` (o `tracepath 8.8.8.8` si traceroute no está
   instalado)
2. Observa cada salto e identifica:
   - ¿Cuántos routers intermedios hay?
   - ¿Cambia la IP origen en cada salto? ¿Por qué o por qué no?
   - ¿En qué capa opera cada router para reenviar tu paquete?
3. Ahora ejecuta `ping -c 3 8.8.8.8` y observa el TTL de la respuesta
4. Calcula cuántos saltos recorrió el paquete de respuesta:
   ```
   TTL inicial típico: 64 (Linux) o 128 (Windows)
   Saltos = TTL_inicial - TTL_recibido
   ```

**Pregunta de reflexión**: si el TTL llega a 0 antes de alcanzar el destino, ¿qué
ocurre? ¿Qué protocolo se usa para notificar al emisor? ¿En qué capa opera ese
protocolo?

### Ejercicio 3: MAC vs IP en acción

1. Haz ping a tu gateway: `ping -c 1 192.168.1.1` (ajusta la IP a tu gateway)
2. Inmediatamente después, revisa la tabla ARP: `ip neigh show`
3. Identifica la MAC address de tu router/gateway
4. Ahora haz ping a un host remoto: `ping -c 1 8.8.8.8`
5. Revisa `ip neigh show` de nuevo

**Pregunta de predicción**: ¿aparecerá la MAC de `8.8.8.8` en tu tabla ARP? ¿Por qué
sí o por qué no?

**Pregunta de reflexión**: cuando envías un paquete a `8.8.8.8`, el frame Ethernet
tiene como MAC destino la MAC de tu **router**, no la MAC del servidor de Google. ¿Por
qué? Relaciona esto con la diferencia entre "entregar al vecino" (enlace) y "entregar
al destino final" (red).
