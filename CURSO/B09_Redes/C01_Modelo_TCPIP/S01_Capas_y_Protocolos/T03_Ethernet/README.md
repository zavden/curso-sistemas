# Ethernet: Frames, MAC Addresses, ARP y ARP Cache

## Índice

1. [Qué es Ethernet](#1-qué-es-ethernet)
2. [Direcciones MAC](#2-direcciones-mac)
3. [El frame Ethernet](#3-el-frame-ethernet)
4. [El problema de la dirección: ¿quién tiene esta IP?](#4-el-problema-de-la-dirección-quién-tiene-esta-ip)
5. [ARP (Address Resolution Protocol)](#5-arp-address-resolution-protocol)
6. [ARP cache](#6-arp-cache)
7. [ARP en acción en Linux](#7-arp-en-acción-en-linux)
8. [Broadcast y dominio de colisión](#8-broadcast-y-dominio-de-colisión)
9. [Switches vs Hubs](#9-switches-vs-hubs)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué es Ethernet

Ethernet (IEEE 802.3) es el protocolo de capa de enlace más utilizado en redes
cableadas. Define cómo los dispositivos de una misma red local (LAN) se comunican
entre sí: el formato de los frames, las direcciones que usa (MAC), y las reglas de
acceso al medio.

```
Red local Ethernet:

[PC A]──┐
         ├──[Switch]──[Router]──── Internet
[PC B]──┘      │
              [PC C]

Dentro de esta LAN, toda la comunicación usa frames Ethernet.
El router traduce entre Ethernet local e Internet.
```

### Velocidades Ethernet

| Estándar | Velocidad | Nombre común | Cable típico |
|---|---|---|---|
| 10BASE-T | 10 Mbps | Ethernet | Cat 3 |
| 100BASE-TX | 100 Mbps | Fast Ethernet | Cat 5 |
| 1000BASE-T | 1 Gbps | Gigabit Ethernet | Cat 5e/6 |
| 10GBASE-T | 10 Gbps | 10 Gigabit | Cat 6a/7 |
| 25GBASE | 25 Gbps | 25 Gigabit | SFP28 (fibra/DAC) |

La velocidad ha cambiado drásticamente, pero el **formato del frame** y el uso de
**direcciones MAC** se mantienen desde los años 80.

---

## 2. Direcciones MAC

### Formato

Una dirección MAC (Media Access Control) es un identificador de 48 bits (6 bytes)
asignado a cada interfaz de red.

```
  aa:bb:cc:dd:ee:ff
  ──────── ────────
     │        │
     │        └── Identificador del dispositivo (asignado por fabricante)
     └─────────── OUI (Organizationally Unique Identifier, asignado por IEEE)

Ejemplo real:  08:00:27:a1:b2:c3
               ────── ──────────
               Oracle  Dispositivo específico
              (VirtualBox)
```

### Características

- **48 bits** = 6 bytes = 12 dígitos hexadecimales
- **Separadores**: `:` en Linux, `-` en Windows (`08-00-27-A1-B2-C3`)
- **Grabada en hardware** (en la ROM de la NIC), aunque se puede cambiar por software
- **Única globalmente** en teoría (los fabricantes registran su OUI ante IEEE)
- **Alcance local**: solo tiene sentido dentro de la misma red de capa 2 (LAN)

### Direcciones MAC especiales

```
ff:ff:ff:ff:ff:ff    Broadcast — todos los dispositivos en la LAN
                     Usado por ARP, DHCP Discover

00:00:00:00:00:00    Dirección nula — sin dirección asignada

01:xx:xx:xx:xx:xx    Multicast (bit menos significativo del primer byte = 1)
                     Usado para protocolos como LLDP, STP
```

### Cómo saber si es unicast, multicast o broadcast

```
Primer byte de la MAC en binario:

  08 = 0000 1000
                ^
                └── Bit 0 = 0 → Unicast (un solo destino)

  01 = 0000 0001
                ^
                └── Bit 0 = 1 → Multicast (grupo de destinos)

  ff = 1111 1111  → Todos los bits = 1 → Broadcast (todos)
```

### Ver tu MAC en Linux

```bash
ip link show
# 2: enp0s3: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel
#     state UP mode DEFAULT group default qlen 1000
#     link/ether 08:00:27:a1:b2:c3 brd ff:ff:ff:ff:ff:ff
#               ─────────────────     ─────────────────
#               Tu MAC                MAC broadcast
```

### Cambiar la MAC (MAC spoofing)

```bash
# Temporal (se pierde al reiniciar)
sudo ip link set enp0s3 down
sudo ip link set enp0s3 address aa:bb:cc:dd:ee:ff
sudo ip link set enp0s3 up

# Verificar
ip link show enp0s3
```

Casos de uso legítimos: testing, clonación de VMs, evadir filtros MAC en redes
públicas. En producción es raro necesitarlo.

---

## 3. El frame Ethernet

### Estructura

```
┌────────────────────────────────────────────────────────────────┐
│                        FRAME ETHERNET                          │
├──────────┬──────────┬───────────┬──────────────────┬──────────┤
│ Dest MAC │ Src MAC  │ EtherType │    Payload        │   FCS    │
│  6 bytes │  6 bytes │  2 bytes  │  46-1500 bytes    │  4 bytes │
└──────────┴──────────┴───────────┴──────────────────┴──────────┘
                                   │                  │
                                   └── MTU (1500 max) ┘
```

### Campos en detalle

| Campo | Tamaño | Descripción |
|---|---|---|
| **Destination MAC** | 6 B | MAC del receptor (o `ff:ff:ff:ff:ff:ff` para broadcast) |
| **Source MAC** | 6 B | MAC del emisor |
| **EtherType** | 2 B | Protocolo del payload: `0x0800`=IPv4, `0x0806`=ARP, `0x86DD`=IPv6 |
| **Payload** | 46-1500 B | El paquete IP (u otro protocolo). Mínimo 46 bytes (padding si es menor) |
| **FCS** | 4 B | Frame Check Sequence — CRC32 para detectar corrupción |

### Tamaño mínimo del frame

```
Frame mínimo Ethernet: 64 bytes (sin contar preámbulo)
  = 6 (dst) + 6 (src) + 2 (type) + 46 (payload mínimo) + 4 (FCS)

Si el payload real es menor que 46 bytes (ej: un paquete ARP de 28 bytes),
se añade PADDING para llegar a 46:

┌──────┬──────┬───────┬──────────┬─────────┬─────┐
│ Dst  │ Src  │ Type  │ ARP (28B)│ Pad(18B)│ FCS │
└──────┴──────┴───────┴──────────┴─────────┴─────┘

¿Por qué mínimo 64 bytes? Razón histórica: detección de colisiones en
Ethernet original (bus compartido) requería que un frame durara lo
suficiente en el cable para detectar la colisión.
```

### Preámbulo y SFD (no visible en software)

Antes del frame hay un preámbulo de 7 bytes (`10101010...`) y un SFD (Start Frame
Delimiter) de 1 byte que sincronizan los relojes del hardware. Son invisibles para
el sistema operativo y las herramientas de captura — tcpdump y Wireshark no los
muestran.

---

## 4. El problema de la dirección: ¿quién tiene esta IP?

Cuando tu máquina quiere enviar un paquete IP a otra máquina en la misma LAN,
necesita construir un frame Ethernet. Tiene la **IP destino** (capa de red) pero
necesita la **MAC destino** (capa de enlace) para crear el frame.

```
Tu PC quiere enviar un paquete a 192.168.1.20:

  Sabe:    IP destino = 192.168.1.20
  Necesita: MAC destino = ???

  Sin la MAC, no puede construir el frame Ethernet.
  ¿Cómo averiguarla?
```

Este es el problema que resuelve ARP.

---

## 5. ARP (Address Resolution Protocol)

### Función

ARP traduce una dirección IP en su dirección MAC correspondiente dentro de la misma
red local. Opera entre la capa de enlace y la capa de red.

### Cómo funciona: ARP Request + ARP Reply

```
Paso 1: ARP Request (broadcast)

Tu PC (192.168.1.10, MAC aa:11:22:33:44:55):
"¿Quién tiene 192.168.1.20? Díganle a aa:11:22:33:44:55"

┌──────────────────┬──────────────────┬──────────────────┐
│ Dst: ff:ff:ff:ff │ Src: aa:11:22:33 │ EtherType: 0x0806│
│ :ff:ff (broadcast)│ :44:55           │ (ARP)            │
├──────────────────┴──────────────────┴──────────────────┤
│ ARP: who-has 192.168.1.20 tell 192.168.1.10            │
└────────────────────────────────────────────────────────┘
         │
         ▼ (todos en la LAN reciben el broadcast)

[PC B: 192.168.1.15]  → "No soy yo" → ignora
[PC C: 192.168.1.20]  → "¡Soy yo!" → responde
[PC D: 192.168.1.30]  → "No soy yo" → ignora
```

```
Paso 2: ARP Reply (unicast)

PC C (192.168.1.20, MAC bb:66:77:88:99:aa):
"192.168.1.20 está en bb:66:77:88:99:aa"

┌──────────────────┬──────────────────┬──────────────────┐
│ Dst: aa:11:22:33 │ Src: bb:66:77:88 │ EtherType: 0x0806│
│ :44:55 (unicast) │ :99:aa           │ (ARP)            │
├──────────────────┴──────────────────┴──────────────────┤
│ ARP: 192.168.1.20 is-at bb:66:77:88:99:aa              │
└────────────────────────────────────────────────────────┘
         │
         ▼ (solo va a tu PC, no broadcast)

Tu PC guarda: 192.168.1.20 → bb:66:77:88:99:aa en la ARP cache
```

### El paquete ARP

```
┌────────────────────────────────────────────────────────────┐
│                    ARP PACKET (28 bytes)                    │
├───────────────────┬────────────────────────────────────────┤
│ Hardware Type (2B)│ 0x0001 = Ethernet                      │
│ Protocol Type (2B)│ 0x0800 = IPv4                          │
│ HW Addr Len  (1B)│ 6 (MAC = 6 bytes)                      │
│ Proto Addr Len(1B)│ 4 (IPv4 = 4 bytes)                    │
│ Operation    (2B)│ 1 = Request, 2 = Reply                  │
├───────────────────┼────────────────────────────────────────┤
│ Sender MAC   (6B)│ MAC del que pregunta/responde           │
│ Sender IP    (4B)│ IP del que pregunta/responde            │
│ Target MAC   (6B)│ 00:00:00:00:00:00 en request           │
│ Target IP    (4B)│ IP que se busca                         │
└───────────────────┴────────────────────────────────────────┘
```

### ARP para el gateway

Cuando envías un paquete a una IP **fuera de tu LAN** (ej: `8.8.8.8`), tu máquina
no hace ARP para `8.8.8.8`. En su lugar:

```
1. Tu PC mira la tabla de routing → 8.8.8.8 no está en mi red local
2. Usa la ruta por defecto → gateway 192.168.1.1
3. Hace ARP para 192.168.1.1 (el router)
4. Construye el frame con:
   - MAC destino: MAC del router
   - IP destino: 8.8.8.8 (se preserva)
5. El router recibe el frame, extrae el paquete IP, y lo reenvía

El frame es para el router (MAC local).
El paquete es para Google (IP global).
```

### Gratuitous ARP

Un dispositivo puede enviar un ARP reply **sin que nadie lo haya pedido**. Se llama
Gratuitous ARP y se usa para:

```
Usos de Gratuitous ARP:
1. Anunciar su presencia al conectarse a la red
2. Actualizar la ARP cache de otros dispositivos tras cambiar de IP
3. Detectar conflictos de IP (si alguien responde, la IP está duplicada)
4. Failover de alta disponibilidad (el servidor backup anuncia que
   ahora él tiene la IP virtual)
```

---

## 6. ARP cache

### Qué es

Cada host mantiene una tabla (cache) de asociaciones IP → MAC aprendidas por ARP.
Esto evita hacer un broadcast ARP por cada paquete que envía.

```
ARP Cache:
┌─────────────────┬───────────────────┬────────────┐
│ IP              │ MAC               │ Estado     │
├─────────────────┼───────────────────┼────────────┤
│ 192.168.1.1     │ aa:bb:cc:11:22:33 │ REACHABLE  │
│ 192.168.1.20    │ bb:66:77:88:99:aa │ REACHABLE  │
│ 192.168.1.25    │ cc:dd:ee:ff:00:11 │ STALE      │
└─────────────────┴───────────────────┴────────────┘
```

### Estados de las entradas

| Estado | Significado |
|---|---|
| **REACHABLE** | Confirmada recientemente (comunicación exitosa) |
| **STALE** | No confirmada hace tiempo — se usará pero se verificará en la próxima oportunidad |
| **DELAY** | Enviado un probe de confirmación, esperando respuesta |
| **FAILED** | No se pudo resolver (host no respondió) |
| **PERMANENT** | Entrada estática, no expira |

### Tiempos de expiración

Las entradas dinámicas tienen un tiempo de vida (normalmente 30-300 segundos según
el estado). Cuando expiran, se eliminan y el próximo paquete a esa IP desencadenará
un nuevo ARP request.

```
Flujo de una entrada ARP:

  ARP reply recibido → REACHABLE (timeout ~30s)
       │
       ▼ (sin tráfico al host)
     STALE (aún usable, pero se verificará)
       │
       ▼ (se necesita enviar un paquete)
     DELAY (probe enviado, esperando confirmación)
       │
       ├──► Confirmación recibida → REACHABLE
       └──► Sin respuesta → FAILED → entrada eliminada
```

---

## 7. ARP en acción en Linux

### Ver la ARP cache

```bash
# Método moderno (ip neigh = ip neighbour)
ip neigh show
# 192.168.1.1 dev enp0s3 lladdr aa:bb:cc:11:22:33 REACHABLE
# 192.168.1.20 dev enp0s3 lladdr bb:66:77:88:99:aa STALE

# Método clásico (aún funciona)
arp -n
# Address          HWtype  HWaddress           Flags  Iface
# 192.168.1.1      ether   aa:bb:cc:11:22:33   C      enp0s3
```

### Manipular la ARP cache

```bash
# Añadir entrada estática (permanente)
sudo ip neigh add 192.168.1.50 lladdr dd:ee:ff:00:11:22 dev enp0s3 nud permanent

# Eliminar una entrada
sudo ip neigh del 192.168.1.50 dev enp0s3

# Vaciar toda la cache
sudo ip neigh flush all
```

### Enviar un ARP request manual

```bash
# arping: envía ARP requests y muestra las respuestas
sudo arping -c 3 -I enp0s3 192.168.1.1
# ARPING 192.168.1.1 from 192.168.1.10 enp0s3
# Unicast reply from 192.168.1.1 [aa:bb:cc:11:22:33]  0.892ms
# Unicast reply from 192.168.1.1 [aa:bb:cc:11:22:33]  0.765ms
```

Usos de arping:
- Verificar que un host está vivo en la capa 2 (sin depender de IP/ICMP)
- Detectar IPs duplicadas
- Diagnosticar cuando ping falla pero sospechas que es un problema de capa superior

### Capturar ARP con tcpdump

```bash
sudo tcpdump -i enp0s3 -n arp
# 08:30:01 ARP, Request who-has 192.168.1.20 tell 192.168.1.10, length 28
# 08:30:01 ARP, Reply 192.168.1.20 is-at bb:66:77:88:99:aa, length 28
```

---

## 8. Broadcast y dominio de colisión

### Dominio de broadcast

Un dominio de broadcast es el conjunto de dispositivos que reciben un frame broadcast
(`ff:ff:ff:ff:ff:ff`). Todos los dispositivos conectados al mismo switch (o conjunto
de switches interconectados) sin VLANs están en el mismo dominio de broadcast.

```
Dominio de broadcast:
┌──────────────────────────────────────────┐
│                                          │
│  [PC A]──┐                               │
│           ├──[Switch 1]──[Switch 2]──┐   │
│  [PC B]──┘                    │      │   │
│                            [PC C] [PC D] │
│                                          │
└──────────────────────────────────────────┘

Si PC A envía un broadcast, PC B, PC C y PC D lo reciben.
Todo está en el mismo dominio de broadcast.
```

### El problema del broadcast excesivo

```
Red con 500 dispositivos en el mismo dominio de broadcast:

  Cada dispositivo hace ARP para resolver direcciones
  + DHCP discovers + NetBIOS + mDNS + ...

  = Cientos de broadcasts por segundo
  = Todos los dispositivos procesan TODOS los broadcasts
  = CPU desperdiciada, ancho de banda consumido

Solución: segmentar la red con VLANs o subredes
  (cada VLAN es un dominio de broadcast independiente)
```

### Dominio de colisión (histórico)

En Ethernet original (con hubs o cable coaxial), todos los dispositivos compartían
el mismo cable. Si dos transmitían a la vez, los datos se corrompían (colisión). El
protocolo CSMA/CD gestionaba esto.

```
Hub (dominio de colisión compartido):
  [A]──┐
       ├──[Hub]    ← Si A y B transmiten a la vez: COLISIÓN
  [B]──┘

Switch (cada puerto es su propio dominio de colisión):
  [A]──┐
       ├──[Switch] ← A y B pueden transmitir simultáneamente
  [B]──┘             El switch gestiona el tráfico por puerto
```

Con switches modernos (full-duplex), las colisiones son prácticamente inexistentes.
CSMA/CD solo es relevante para entender la historia de Ethernet y por qué existe el
tamaño mínimo de frame de 64 bytes.

---

## 9. Switches vs Hubs

### Hub (capa 1 — repetidor)

```
[A]──┐
      ├──[Hub]──[C]
[B]──┘

A envía un frame a C:
  Hub repite el frame por TODOS los puertos
  → B también recibe el frame (y lo descarta si no es para su MAC)
  → Ineficiente, inseguro, colisiones posibles
```

### Switch (capa 2 — conmutación)

```
[A]──┐
      ├──[Switch]──[C]
[B]──┘

A envía un frame a C:
  Switch lee la MAC destino
  Switch consulta su MAC address table
  Switch envía el frame SOLO al puerto de C
  → B nunca ve el frame
  → Eficiente, más seguro, sin colisiones
```

### MAC address table del switch

El switch aprende qué MAC está conectada a qué puerto observando los frames:

```
MAC Address Table:
┌───────────────────┬────────┐
│ MAC               │ Puerto │
├───────────────────┼────────┤
│ aa:11:22:33:44:55 │ Port 1 │  (aprendida de frames de A)
│ bb:66:77:88:99:aa │ Port 2 │  (aprendida de frames de B)
│ cc:dd:ee:ff:00:11 │ Port 3 │  (aprendida de frames de C)
└───────────────────┴────────┘

Si el switch recibe un frame con MAC destino desconocida:
  → Flooding: envía por todos los puertos (como un hub)
  → Cuando el destino responde, el switch aprende su puerto
```

### Implicación para la seguridad

En un switch, normalmente no puedes capturar tráfico de otros hosts (a diferencia de
un hub). Para capturar tráfico ajeno necesitas:
- **Port mirroring** (SPAN): el switch copia tráfico de un puerto a otro
- **ARP spoofing**: engañar a otros hosts para que te envíen su tráfico (ataque)
- **TAP de red**: dispositivo hardware que copia tráfico pasivamente

---

## 10. Errores comunes

### Error 1: Pensar que la MAC viaja de extremo a extremo

```
PC A ──── Router 1 ──── Router 2 ──── PC B

Frame 1 (A → Router 1):
  Src MAC: MAC de A        Dst MAC: MAC de Router 1

Frame 2 (Router 1 → Router 2):
  Src MAC: MAC de Router 1  Dst MAC: MAC de Router 2

Frame 3 (Router 2 → B):
  Src MAC: MAC de Router 2  Dst MAC: MAC de B

La MAC cambia en CADA SALTO. Solo la IP se preserva de extremo a extremo.
```

### Error 2: Confundir ARP broadcast con IP broadcast

```
ARP broadcast = frame Ethernet con MAC destino ff:ff:ff:ff:ff:ff
                Capa 2 (enlace)
                Solo llega al dominio de broadcast local

IP broadcast  = paquete IP con IP destino 255.255.255.255 o broadcast de subred
                Capa 3 (red)
                También se implementa sobre MAC broadcast,
                pero es un concepto de capa diferente
```

### Error 3: Intentar hacer ARP a una IP fuera de tu subred

```bash
# Tu IP: 192.168.1.10/24
# Intentas resolver la MAC de 10.0.0.5 (otra red)

arping 10.0.0.5
# No funciona — 10.0.0.5 no está en tu LAN
# ARP solo funciona dentro de la misma red de capa 2
```

Para llegar a `10.0.0.5`, tu máquina hace ARP para el **gateway** y envía el paquete
al router, que se encarga del resto.

### Error 4: No vaciar la ARP cache después de cambiar la IP de un host

```
Escenario:
  Servidor tenía IP 192.168.1.50 con MAC aa:bb:cc:dd:ee:ff
  Cambias el servidor, el nuevo tiene IP 192.168.1.50 pero MAC diferente
  Otros hosts siguen con la MAC antigua en su ARP cache
  → No pueden comunicarse con el nuevo servidor

Solución:
  Esperar a que la cache expire (~minutos)
  O vaciar manualmente: sudo ip neigh flush all
  O el nuevo servidor envía un Gratuitous ARP al arrancar
```

### Error 5: Ignorar ARP en diagnóstico de red

```
Síntoma: "ping 192.168.1.20 falla"
Diagnóstico típico: "el host está caído"

Pero podría ser un problema de ARP:
  - IP duplicada (dos hosts con la misma IP → respuestas contradictorias)
  - ARP cache envenenada (ARP spoofing)
  - Host con firewall que bloquea ARP (raro pero posible)

Verificar:
  ip neigh show | grep 192.168.1.20
  sudo arping -c 1 192.168.1.20
  sudo tcpdump -i enp0s3 arp
```

---

## 11. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║                ETHERNET Y ARP — REFERENCIA RÁPIDA                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                      ║
║  DIRECCIONES MAC                                                     ║
║  Formato:        aa:bb:cc:dd:ee:ff (48 bits, 6 bytes)                ║
║  Broadcast:      ff:ff:ff:ff:ff:ff                                   ║
║  Ver la tuya:    ip link show                                        ║
║  Cambiar:        ip link set dev address XX:XX:XX:XX:XX:XX           ║
║                                                                      ║
║  FRAME ETHERNET                                                      ║
║  ┌────────┬────────┬──────────┬──────────────┬─────┐                ║
║  │Dst MAC │Src MAC │EtherType │ Payload      │ FCS │                ║
║  │ 6B     │ 6B     │ 2B       │ 46-1500B     │ 4B  │                ║
║  └────────┴────────┴──────────┴──────────────┴─────┘                ║
║  EtherType: 0x0800=IPv4  0x0806=ARP  0x86DD=IPv6                    ║
║  Frame mínimo: 64 bytes (padding si payload < 46B)                   ║
║                                                                      ║
║  ARP                                                                 ║
║  Función:   IP → MAC (dentro de la misma LAN)                       ║
║  Request:   broadcast "¿quién tiene IP X?"                           ║
║  Reply:     unicast "IP X está en MAC Y"                             ║
║  Gratuitous: reply no solicitado (anuncia presencia)                 ║
║                                                                      ║
║  ARP CACHE                                                           ║
║  ip neigh show                    Ver cache                          ║
║  ip neigh flush all               Vaciar cache                       ║
║  ip neigh add IP lladdr MAC       Entrada estática                   ║
║    dev DEV nud permanent                                             ║
║  ip neigh del IP dev DEV          Eliminar entrada                   ║
║                                                                      ║
║  DIAGNÓSTICO                                                         ║
║  arping -c 3 -I DEV IP           ARP manual                         ║
║  tcpdump -i DEV -n arp           Capturar tráfico ARP               ║
║                                                                      ║
║  ESTADOS DE ARP CACHE                                                ║
║  REACHABLE → confirmada recientemente                                ║
║  STALE     → no confirmada, se verificará al usar                    ║
║  DELAY     → probe enviado, esperando respuesta                      ║
║  FAILED    → host no respondió                                       ║
║  PERMANENT → estática, no expira                                     ║
║                                                                      ║
║  RECUERDA                                                            ║
║  • MAC = alcance local (cambia en cada salto)                        ║
║  • ARP = solo funciona dentro de la misma LAN                        ║
║  • Para IPs remotas, se hace ARP al gateway, no al destino           ║
║  • Switch aprende MACs → envía solo al puerto correcto               ║
║  • Hub repite todo → ineficiente, obsoleto                           ║
║                                                                      ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 12. Ejercicios

### Ejercicio 1: Explorar ARP en tu red

1. Vacía la ARP cache:
   ```bash
   sudo ip neigh flush all
   ```
2. Verifica que está vacía:
   ```bash
   ip neigh show
   ```
3. Haz ping a tu gateway:
   ```bash
   ping -c 1 $(ip route show default | awk '{print $3}')
   ```
4. Revisa la ARP cache ahora:
   ```bash
   ip neigh show
   ```
5. Abre tcpdump en otra terminal para capturar ARP:
   ```bash
   sudo tcpdump -i enp0s3 -n arp -c 4
   ```
6. Vacía la cache de nuevo y haz ping a otro host de tu red (o al gateway)
7. Observa en tcpdump el ARP request (broadcast) y el ARP reply (unicast)

**Pregunta de predicción**: después del primer ping al gateway, ¿cuántas entradas
esperas ver en la ARP cache? Si haces un segundo ping inmediatamente, ¿verás un
nuevo ARP request en tcpdump?

**Pregunta de reflexión**: ¿por qué el ARP request es broadcast pero el ARP reply
es unicast? ¿Podría funcionar al revés?

### Ejercicio 2: Identificar el OUI de tus dispositivos

1. Lista todas las entradas en tu ARP cache:
   ```bash
   ip neigh show
   ```
2. Para cada MAC que veas, identifica el fabricante usando los primeros 3 bytes
   (OUI). Puedes buscarlos en línea o con:
   ```bash
   # Si tienes el paquete ieee-data instalado
   grep -i "08:00:27" /usr/share/ieee-data/oui.txt
   ```
3. Identifica la MAC de tu gateway — ¿qué fabricante es? ¿Coincide con la marca
   de tu router?
4. Identifica tu propia MAC — si estás en una VM, ¿qué OUI ves? ¿Coincide con
   el hipervisor?

**Pregunta de reflexión**: si estás en una VM con VirtualBox, verás el OUI de Oracle
(`08:00:27`). ¿Por qué la MAC de una VM no es la MAC real de tu hardware? ¿Quién
asigna la MAC de una VM?

### Ejercicio 3: ARP y el gateway

1. Vacía la ARP cache
2. Haz ping a una IP remota (no en tu LAN):
   ```bash
   ping -c 1 8.8.8.8
   ```
3. Revisa la ARP cache:
   ```bash
   ip neigh show
   ```
4. Responde:
   - ¿Aparece la MAC de `8.8.8.8` en tu ARP cache?
   - ¿Qué entrada SÍ aparece?
   - ¿De quién es esa MAC?

5. Ahora haz ping a un segundo host remoto:
   ```bash
   ping -c 1 1.1.1.1
   ```
6. Revisa la ARP cache otra vez — ¿apareció una nueva entrada para `1.1.1.1`?

**Pregunta de reflexión**: todos los paquetes a IPs remotas pasan por el gateway.
Eso significa que para la capa de enlace, el **único vecino** de tu PC es el router.
¿Qué pasaría si el router se apaga? ¿Podrías seguir comunicándote con otros hosts
de tu LAN? ¿Y con hosts remotos? ¿Por qué la respuesta es diferente?
