# T01 — El Comando `ip`: ip addr, ip route, ip link, ip neigh

## Índice

1. [De net-tools a iproute2](#1-de-net-tools-a-iproute2)
2. [Estructura del comando ip](#2-estructura-del-comando-ip)
3. [ip link — Interfaces de red](#3-ip-link--interfaces-de-red)
4. [ip addr — Direcciones IP](#4-ip-addr--direcciones-ip)
5. [ip route — Tabla de rutas](#5-ip-route--tabla-de-rutas)
6. [ip neigh — Tabla ARP / vecinos](#6-ip-neigh--tabla-arp--vecinos)
7. [ip -s — Estadísticas](#7-ip--s--estadísticas)
8. [ip monitor — Eventos en tiempo real](#8-ip-monitor--eventos-en-tiempo-real)
9. [Formatos de salida](#9-formatos-de-salida)
10. [Persistencia: los cambios no sobreviven al reinicio](#10-persistencia-los-cambios-no-sobreviven-al-reinicio)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. De net-tools a iproute2

Durante décadas, los comandos `ifconfig`, `route`, `arp` y `netstat` (paquete **net-tools**) fueron las herramientas estándar para configurar redes en Linux. Sin embargo, net-tools dejó de mantenerse y no soporta funcionalidades modernas del kernel:

```
net-tools (legacy)              iproute2 (moderno)
┌──────────────┐                ┌──────────────┐
│ ifconfig     │ ──────────►   │ ip addr      │
│ route        │ ──────────►   │ ip route     │
│ arp          │ ──────────►   │ ip neigh     │
│ netstat      │ ──────────►   │ ss           │
│ ifconfig up  │ ──────────►   │ ip link set  │
│ brctl        │ ──────────►   │ ip link/bridge│
│ iptunnel     │ ──────────►   │ ip tunnel    │
│ vconfig      │ ──────────►   │ ip link (vlan)│
└──────────────┘                └──────────────┘
  Sin mantenimiento               Desarrollo activo
  No soporta netns                Soporta namespaces
  Limitado con IPv6               Soporte completo IPv6
  Ioctl interface                 Netlink interface
```

**¿Por qué migrar?** No es solo una cuestión de "nuevo vs viejo":

| Limitación de net-tools | Solución en iproute2 |
|-------------------------|----------------------|
| `ifconfig` no muestra direcciones secundarias correctamente | `ip addr` muestra todas las IPs de una interfaz |
| `route` no soporta rutas avanzadas (source routing, policy) | `ip route` / `ip rule` soporta todo el stack de routing |
| `arp` no distingue estados de vecinos | `ip neigh` muestra STALE, REACHABLE, DELAY, etc. |
| No funciona con network namespaces | `ip netns` gestión completa de namespaces |
| No soporta VLANs, bonds, bridges nativamente | `ip link add type {vlan,bond,bridge}` |

> **Importante**: muchas distribuciones modernas ya no instalan net-tools por defecto. Ubuntu Server, Fedora, y contenedores típicamente solo tienen iproute2.

---

## 2. Estructura del comando ip

Todos los subcomandos de `ip` siguen una estructura coherente:

```
ip [OPTIONS] OBJECT {COMMAND | help}

Opciones globales:
  -4          Solo IPv4
  -6          Solo IPv6
  -s          Estadísticas
  -d          Detalles
  -j          Salida JSON
  -p          Pretty-print (con -j)
  -br         Salida breve (brief)
  -c          Colores
  -o          Una línea por registro (oneline)
  -n          Network namespace

Objetos principales:
  link        Interfaces de red (capa 2)
  addr        Direcciones IP (capa 3)
  route       Tabla de enrutamiento
  neigh       Tabla de vecinos (ARP/NDP)
  rule        Policy routing rules
  netns       Network namespaces
  tunnel      Túneles IP
  maddress    Direcciones multicast
```

```
Jerarquía conceptual:

              ip
              │
    ┌─────────┼──────────┬──────────┐
    │         │          │          │
  link      addr      route      neigh
  (L2)      (L3)      (L3)      (L2/L3)
    │         │          │          │
  ¿Existe? ¿Qué IP?  ¿A dónde?  ¿Quién está
  ¿UP?     ¿Máscara?  ¿Gateway?  cerca?
  ¿MTU?    ¿Scope?    ¿Metric?   ¿MAC↔IP?
```

Los subcomandos se pueden abreviar. `ip` acepta la forma más corta que no sea ambigua:

```bash
ip address show    # Completo
ip addr show       # Abreviado
ip a s             # Mínimo
ip a               # Aún más corto (show es default)

ip link show       # Completo
ip l               # Mínimo

ip route show      # Completo
ip r               # Mínimo

ip neighbour show  # Completo
ip neigh show      # Abreviado
ip n               # Mínimo
```

---

## 3. ip link — Interfaces de red

`ip link` opera a **nivel de enlace (capa 2)**. Gestiona las interfaces de red: crear, eliminar, activar, desactivar, cambiar MTU, renombrar.

### 3.1 Listar interfaces

```bash
# Todas las interfaces
ip link show
```

```
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN mode DEFAULT group default qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP mode DEFAULT group default qlen 1000
    link/ether aa:bb:cc:11:22:33 brd ff:ff:ff:ff:ff:ff
3: wlan0: <BROADCAST,MULTICAST> mtu 1500 qdisc noop state DOWN mode DEFAULT group default qlen 1000
    link/ether dd:ee:ff:44:55:66 brd ff:ff:ff:ff:ff:ff
```

Desglose de la salida:

```
2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP ...
│   │          │                            │         │             │
│   │          │                            │         │             └── state: UP/DOWN
│   │          │                            │         └── Queueing discipline
│   │          │                            └── Maximum Transmission Unit
│   │          └── Flags (capacidades y estado)
│   └── Nombre de la interfaz
└── Índice numérico

    link/ether aa:bb:cc:11:22:33 brd ff:ff:ff:ff:ff:ff
               │                     │
               └── MAC address       └── Broadcast MAC
```

**Flags importantes**:

| Flag | Significado |
|------|-------------|
| `UP` | Interfaz activada administrativamente (ip link set up) |
| `LOWER_UP` | Cable conectado / señal presente (capa física) |
| `BROADCAST` | Soporta broadcast |
| `MULTICAST` | Soporta multicast |
| `NO-CARRIER` | Sin señal (cable desconectado) |
| `PROMISC` | Modo promiscuo activado |

```
Estado real de la interfaz:

UP + LOWER_UP     → Funcionando (cable conectado, activada)
UP + NO-CARRIER   → Activada pero sin cable / sin señal
DOWN               → Desactivada administrativamente
```

```bash
# Formato breve (más legible)
ip -br link show
```

```
lo               UNKNOWN        00:00:00:00:00:00 <LOOPBACK,UP,LOWER_UP>
eth0             UP             aa:bb:cc:11:22:33 <BROADCAST,MULTICAST,UP,LOWER_UP>
wlan0            DOWN           dd:ee:ff:44:55:66 <BROADCAST,MULTICAST>
```

```bash
# Una interfaz específica
ip link show eth0

# Solo interfaces UP
ip link show up

# Filtrar por tipo
ip link show type bridge
ip link show type vlan
ip link show type bond
```

### 3.2 Activar y desactivar interfaces

```bash
# Activar interfaz (equivalente a ifconfig eth0 up)
sudo ip link set eth0 up

# Desactivar interfaz
sudo ip link set eth0 down
```

### 3.3 Cambiar propiedades

```bash
# Cambiar MTU
sudo ip link set eth0 mtu 9000    # Jumbo frames

# Cambiar MAC address (interfaz debe estar DOWN)
sudo ip link set eth0 down
sudo ip link set eth0 address 00:11:22:33:44:55
sudo ip link set eth0 up

# Cambiar nombre de interfaz (debe estar DOWN)
sudo ip link set eth0 down
sudo ip link set eth0 name lan0
sudo ip link set lan0 up

# Activar modo promiscuo (capturar todo el tráfico del segmento)
sudo ip link set eth0 promisc on

# Cambiar queueing discipline
sudo ip link set eth0 txqueuelen 2000
```

### 3.4 Crear y eliminar interfaces virtuales

```bash
# Crear interfaz VLAN (802.1Q)
sudo ip link add link eth0 name eth0.100 type vlan id 100
sudo ip link set eth0.100 up

# Crear bridge
sudo ip link add name br0 type bridge
sudo ip link set br0 up
sudo ip link set eth0 master br0

# Crear interfaz dummy (testing)
sudo ip link add name dummy0 type dummy
sudo ip link set dummy0 up

# Crear par veth (para contenedores/namespaces)
sudo ip link add veth0 type veth peer name veth1

# Eliminar interfaz virtual
sudo ip link del eth0.100
sudo ip link del br0
```

---

## 4. ip addr — Direcciones IP

`ip addr` opera a **nivel de red (capa 3)**. Gestiona las direcciones IP asignadas a las interfaces.

### 4.1 Listar direcciones

```bash
# Todas las direcciones de todas las interfaces
ip addr show
```

```
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN group default qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    inet 127.0.0.1/8 scope host lo
       valid_lft forever preferred_lft forever
    inet6 ::1/128 scope host noprefixroute
       valid_lft forever preferred_lft forever
2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP group default qlen 1000
    link/ether aa:bb:cc:11:22:33 brd ff:ff:ff:ff:ff:ff
    inet 192.168.1.100/24 brd 192.168.1.255 scope global dynamic eth0
       valid_lft 25200sec preferred_lft 25200sec
    inet6 fe80::a8bb:ccff:fe11:2233/64 scope link
       valid_lft forever preferred_lft forever
```

Desglose:

```
inet 192.168.1.100/24 brd 192.168.1.255 scope global dynamic eth0
│    │              │  │                 │           │
│    │              │  │                 │           └── dynamic = obtenida por DHCP
│    │              │  │                 └── scope: global/link/host
│    │              │  └── Dirección broadcast
│    │              └── Prefijo CIDR (máscara)
│    └── Dirección IP
└── inet = IPv4 (inet6 = IPv6)

   valid_lft 25200sec preferred_lft 25200sec
   │                  │
   └── Valid lifetime  └── Preferred lifetime
       (DHCP lease)        (DHCPv6 / SLAAC)
```

**Scope** indica el alcance de la dirección:

| Scope | Significado | Ejemplo |
|-------|-------------|---------|
| `global` | Válida en todas las redes | 192.168.1.100 |
| `link` | Solo válida en el segmento local | fe80::... (link-local) |
| `host` | Solo válida en esta máquina | 127.0.0.1 |

```bash
# Formato breve
ip -br addr show
```

```
lo               UNKNOWN        127.0.0.1/8 ::1/128
eth0             UP             192.168.1.100/24 fe80::a8bb:ccff:fe11:2233/64
wlan0            DOWN
```

```bash
# Solo IPv4
ip -4 addr show

# Solo IPv6
ip -6 addr show

# Solo una interfaz
ip addr show eth0

# Solo direcciones con scope global
ip addr show scope global

# Solo direcciones dinámicas (DHCP)
ip addr show dynamic

# Solo direcciones permanentes (estáticas)
ip addr show permanent
```

### 4.2 Agregar y eliminar direcciones

```bash
# Agregar dirección IP
sudo ip addr add 192.168.1.50/24 dev eth0

# Agregar dirección con broadcast explícito
sudo ip addr add 192.168.1.50/24 brd 192.168.1.255 dev eth0

# Agregar dirección con label (para compatibilidad con ifconfig)
sudo ip addr add 192.168.1.51/24 dev eth0 label eth0:1

# Eliminar dirección
sudo ip addr del 192.168.1.50/24 dev eth0

# Eliminar TODAS las direcciones de una interfaz
sudo ip addr flush dev eth0

# Eliminar solo IPv4 de una interfaz
sudo ip -4 addr flush dev eth0
```

**Múltiples IPs en una interfaz**: a diferencia de `ifconfig`, que requería alias (eth0:0, eth0:1), `ip addr` permite múltiples direcciones en la misma interfaz directamente:

```bash
# Agregar varias IPs a eth0
sudo ip addr add 192.168.1.50/24 dev eth0
sudo ip addr add 192.168.1.51/24 dev eth0
sudo ip addr add 10.0.0.1/24 dev eth0

# ip addr show eth0 las muestra todas:
# inet 192.168.1.100/24 scope global dynamic eth0
# inet 192.168.1.50/24 scope global eth0
# inet 192.168.1.51/24 scope global eth0
# inet 10.0.0.1/24 scope global eth0
```

### 4.3 Dirección primaria vs secundaria

La primera dirección añadida a una interfaz (en una subred dada) es la **primaria**. Las siguientes en la misma subred son **secundarias**:

```
eth0:
  inet 192.168.1.100/24  ← Primaria (primera en esta subred)
  inet 192.168.1.50/24   ← Secundaria (misma subred)
  inet 10.0.0.1/24       ← Primaria (primera en ESTA subred)
```

Esto importa porque si eliminas la dirección primaria, **todas las secundarias de esa subred se eliminan también** (comportamiento por defecto del kernel). Para evitarlo:

```bash
# Promover secundaria a primaria al eliminar la primaria
sudo sysctl -w net.ipv4.conf.eth0.promote_secondaries=1
```

---

## 5. ip route — Tabla de rutas

`ip route` gestiona la tabla de enrutamiento del kernel. Determina **a dónde enviar** cada paquete basándose en la IP destino.

### 5.1 Ver la tabla de rutas

```bash
# Tabla de rutas principal
ip route show
```

```
default via 192.168.1.1 dev eth0 proto dhcp src 192.168.1.100 metric 100
192.168.1.0/24 dev eth0 proto kernel scope link src 192.168.1.100 metric 100
169.254.0.0/16 dev eth0 scope link metric 1000
```

Desglose:

```
default via 192.168.1.1 dev eth0 proto dhcp src 192.168.1.100 metric 100
│       │               │        │          │                  │
│       │               │        │          │                  └── Prioridad (menor=mejor)
│       │               │        │          └── IP origen preferida
│       │               │        └── Quién creó la ruta
│       │               └── Por qué interfaz sale
│       └── Gateway (next hop)
└── Destino (default = 0.0.0.0/0)

192.168.1.0/24 dev eth0 proto kernel scope link src 192.168.1.100
│              │        │            │
│              │        │            └── scope link = red directamente conectada
│              │        └── proto kernel = creada automáticamente por el kernel
│              └── Sale por eth0 (sin gateway, red local)
└── Subred destino
```

**Protocolos (proto)** — quién instaló la ruta:

| Proto | Significado |
|-------|-------------|
| `kernel` | Creada automáticamente al añadir una IP |
| `dhcp` | Instalada por el cliente DHCP |
| `static` | Añadida manualmente con `ip route add` |
| `ra` | Instalada por Router Advertisement (IPv6) |
| `bird` / `zebra` | Instalada por un daemon de routing |

```bash
# Solo rutas por defecto
ip route show default

# Rutas de una tabla específica
ip route show table main
ip route show table local
ip route show table all

# ¿Por dónde sale un paquete hacia X?
ip route get 8.8.8.8
```

```
# Salida de ip route get 8.8.8.8:
8.8.8.8 via 192.168.1.1 dev eth0 src 192.168.1.100 uid 1000
    cache
```

Este comando es extremadamente útil para debugging: te dice exactamente qué ruta, interfaz y IP origen usará el kernel para llegar a un destino.

```bash
# ¿Por dónde llego a un host local?
ip route get 192.168.1.50
# 192.168.1.50 dev eth0 src 192.168.1.100 uid 1000
# (sin "via" = red directamente conectada, sin gateway)
```

### 5.2 Agregar y eliminar rutas

```bash
# Ruta por defecto (gateway)
sudo ip route add default via 192.168.1.1 dev eth0

# Ruta a una red específica vía gateway
sudo ip route add 10.0.0.0/8 via 192.168.1.254 dev eth0

# Ruta a una red directamente conectada (sin gateway)
sudo ip route add 172.16.0.0/16 dev eth1

# Ruta con metric específica
sudo ip route add default via 192.168.1.1 dev eth0 metric 100

# Ruta con IP origen específica
sudo ip route add 10.0.0.0/8 via 192.168.1.254 src 192.168.1.50

# Eliminar ruta
sudo ip route del 10.0.0.0/8 via 192.168.1.254

# Eliminar ruta por defecto
sudo ip route del default

# Reemplazar ruta (crear si no existe, reemplazar si existe)
sudo ip route replace default via 192.168.1.1 dev eth0
```

### 5.3 Rutas con múltiples gateways

```bash
# Dos rutas por defecto con diferente prioridad (failover)
sudo ip route add default via 192.168.1.1 dev eth0 metric 100
sudo ip route add default via 192.168.2.1 dev eth1 metric 200
# eth0 es preferida (metric menor); eth1 es backup

# Balanceo de carga (multipath)
sudo ip route add default \
    nexthop via 192.168.1.1 dev eth0 weight 1 \
    nexthop via 192.168.2.1 dev eth1 weight 1
```

### 5.4 Blackhole, unreachable, prohibit

```bash
# Blackhole: descartar paquetes silenciosamente
sudo ip route add blackhole 10.99.0.0/16

# Unreachable: responder con ICMP "Network unreachable"
sudo ip route add unreachable 10.99.0.0/16

# Prohibit: responder con ICMP "Communication administratively prohibited"
sudo ip route add prohibit 10.99.0.0/16
```

Útiles para:
- **Blackhole**: mitigar ataques (null routing), descartar tráfico no deseado
- **Unreachable/Prohibit**: informar al emisor que el destino no es alcanzable

### 5.5 Tablas de rutas múltiples y policy routing

Linux soporta hasta 252 tablas de rutas. Esto permite **policy routing**: diferentes decisiones de ruteo según IP origen, puerto, marca del paquete, etc.

```bash
# Ver todas las tablas
ip rule show
```

```
0:      from all lookup local
32766:  from all lookup main
32767:  from all lookup default
```

```bash
# Crear regla: tráfico de 10.0.0.0/8 usa tabla 100
sudo ip rule add from 10.0.0.0/8 table 100

# Agregar ruta en tabla 100
sudo ip route add default via 172.16.0.1 table 100

# Resultado: hosts con IP 10.x salen por 172.16.0.1
#            todos los demás usan la tabla main (gateway normal)
```

---

## 6. ip neigh — Tabla ARP / vecinos

`ip neigh` muestra y gestiona la tabla de vecinos (ARP para IPv4, NDP para IPv6). Esta tabla mapea direcciones IP a direcciones MAC en la red local.

### 6.1 Ver la tabla de vecinos

```bash
ip neigh show
```

```
192.168.1.1 dev eth0 lladdr 00:11:22:33:44:55 REACHABLE
192.168.1.105 dev eth0 lladdr aa:bb:cc:dd:ee:ff STALE
192.168.1.200 dev eth0  FAILED
fe80::1 dev eth0 lladdr 00:11:22:33:44:55 router REACHABLE
```

**Estados de los vecinos**:

```
INCOMPLETE ──► REACHABLE ──► STALE ──► DELAY ──► PROBE ──► FAILED
    │              │            │                    │
    │              │            │                    └── Reintento fallido
    │              │            └── Tiempo sin confirmación
    │              └── Confirmado recientemente
    └── ARP request enviado, esperando respuesta
```

| Estado | Significado |
|--------|-------------|
| `REACHABLE` | MAC confirmada recientemente, entrada válida |
| `STALE` | La entrada no se ha confirmado en un tiempo, pero se usa hasta que falle |
| `DELAY` | Se envió un paquete, esperando confirmación antes de revalidar |
| `PROBE` | Enviando ARP unicast para revalidar la entrada |
| `INCOMPLETE` | Se envió ARP request, sin respuesta aún |
| `FAILED` | ARP request sin respuesta tras múltiples intentos |
| `PERMANENT` | Entrada estática, no expira |
| `NOARP` | Interfaz sin ARP (loopback, tunnels) |

```bash
# Solo IPv4
ip -4 neigh show

# Solo una interfaz
ip neigh show dev eth0

# Solo entradas en estado específico
ip neigh show nud reachable
ip neigh show nud stale
```

### 6.2 Modificar la tabla de vecinos

```bash
# Agregar entrada estática (permanente)
sudo ip neigh add 192.168.1.50 lladdr 00:aa:bb:cc:dd:ee dev eth0 nud permanent

# Eliminar entrada
sudo ip neigh del 192.168.1.50 dev eth0

# Limpiar toda la tabla (forzar re-resolución ARP)
sudo ip neigh flush all
sudo ip neigh flush dev eth0

# Cambiar entrada existente
sudo ip neigh replace 192.168.1.50 lladdr 00:aa:bb:cc:dd:ee dev eth0 nud permanent
```

Las entradas permanentes son útiles para:
- Prevenir ARP spoofing en hosts críticos (gateway)
- Debugging (forzar comunicación con una MAC específica)

```bash
# Proteger la entrada del gateway contra ARP spoofing
sudo ip neigh replace 192.168.1.1 lladdr 00:11:22:33:44:55 dev eth0 nud permanent
```

---

## 7. ip -s — Estadísticas

La flag `-s` (statistics) añade contadores de tráfico a la salida:

```bash
ip -s link show eth0
```

```
2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP mode DEFAULT group default qlen 1000
    link/ether aa:bb:cc:11:22:33 brd ff:ff:ff:ff:ff:ff
    RX:  bytes packets errors dropped  missed   mcast
      1234567   89012      0       0       0     123
    TX:  bytes packets errors dropped carrier collsns
       987654   56789      0       0       0       0
```

| Contador | Significado |
|----------|-------------|
| `RX bytes/packets` | Datos recibidos |
| `TX bytes/packets` | Datos transmitidos |
| `errors` | Errores de transmisión/recepción (hardware, driver) |
| `dropped` | Paquetes descartados (buffer lleno, filtrado) |
| `missed` | Paquetes que el hardware no pudo procesar |
| `carrier` | Errores de portadora (problemas de cable/señal) |
| `collsns` | Colisiones (raro en redes modernas full-duplex) |

```bash
# Estadísticas más detalladas (doble -s)
ip -s -s link show eth0

# Estadísticas de todas las interfaces
ip -s link show

# Comparar estadísticas en el tiempo (monitor manual)
ip -s link show eth0 | grep -A 2 "RX:"
# Esperar...
ip -s link show eth0 | grep -A 2 "RX:"
# Comparar contadores
```

---

## 8. ip monitor — Eventos en tiempo real

`ip monitor` observa cambios en la configuración de red en tiempo real. Extremadamente útil para debugging:

```bash
# Monitorear todo
ip monitor

# Monitorear solo cambios de dirección
ip monitor address

# Monitorear solo cambios de rutas
ip monitor route

# Monitorear solo cambios de link
ip monitor link

# Monitorear solo vecinos (ARP)
ip monitor neigh

# Combinaciones
ip monitor address route link
```

```bash
# Ejemplo: en una terminal, monitorear:
ip monitor address

# En otra terminal, añadir una IP:
sudo ip addr add 10.0.0.5/24 dev eth0

# El monitor muestra:
# [ADDR] 3: eth0    inet 10.0.0.5/24 scope global eth0
#        valid_lft forever preferred_lft forever
```

Casos de uso:
- Detectar cuándo DHCP asigna una nueva IP
- Observar cambios de ruta (gateway cambió)
- Detectar interfaces que caen y suben
- Debugging de NetworkManager / systemd-networkd

---

## 9. Formatos de salida

`ip` soporta varios formatos de salida que facilitan el scripting y la legibilidad:

```bash
# JSON (ideal para scripting con jq)
ip -j addr show eth0
ip -j route show
ip -j neigh show

# JSON con pretty-print
ip -j -p addr show eth0
```

```json
[
    {
        "ifindex": 2,
        "ifname": "eth0",
        "flags": ["BROADCAST","MULTICAST","UP","LOWER_UP"],
        "mtu": 1500,
        "link_type": "ether",
        "address": "aa:bb:cc:11:22:33",
        "addr_info": [
            {
                "family": "inet",
                "local": "192.168.1.100",
                "prefixlen": 24,
                "broadcast": "192.168.1.255",
                "scope": "global",
                "dynamic": true,
                "label": "eth0",
                "valid_life_time": 25200,
                "preferred_life_time": 25200
            }
        ]
    }
]
```

```bash
# Extraer IP con jq
ip -j addr show eth0 | jq -r '.[0].addr_info[] | select(.family=="inet") | .local'
# 192.168.1.100

# Extraer gateway por defecto
ip -j route show default | jq -r '.[0].gateway'
# 192.168.1.1

# Brief (tabla compacta)
ip -br addr show
ip -br link show

# Oneline (una línea por registro, útil para grep)
ip -o addr show
ip -o route show

# Colores (resalta UP/DOWN, estados)
ip -c addr show
ip -c link show
```

Combinaciones para scripting:

```bash
# Listar todas las IPs IPv4 del sistema
ip -j -4 addr show | jq -r '.[] | .addr_info[] | .local'

# Listar interfaces UP con sus IPs
ip -j addr show up | jq -r '.[] | "\(.ifname): \(.addr_info[] | select(.family=="inet") | .local)"'

# Contar vecinos REACHABLE
ip -j neigh show nud reachable | jq length

# ¿Tiene el host ruta al destino?
ip route get 10.0.0.1 > /dev/null 2>&1 && echo "Ruta existe" || echo "Sin ruta"
```

---

## 10. Persistencia: los cambios no sobreviven al reinicio

**Todos los cambios hechos con `ip` son temporales.** Cuando el sistema reinicia (o la interfaz se reconfigura), los cambios se pierden. Esto es a la vez una ventaja (puedes experimentar sin riesgo) y una fuente frecuente de confusión:

```
ip addr add 10.0.0.5/24 dev eth0     ← Inmediato, funciona ahora
                                        Pero desaparece al reiniciar

Para persistir, debes usar el gestor de red:
┌──────────────────────────────────────────────────────┐
│ NetworkManager:                                      │
│   nmcli con mod "X" +ipv4.addresses 10.0.0.5/24     │
│                                                      │
│ systemd-networkd:                                    │
│   [Network]                                          │
│   Address=10.0.0.5/24                                │
│                                                      │
│ ifupdown:                                            │
│   address 10.0.0.5/24  (en /etc/network/interfaces)  │
└──────────────────────────────────────────────────────┘
```

El flujo correcto de trabajo:

```
1. Experimentar con ip (temporal):
   sudo ip addr add 10.0.0.5/24 dev eth0
   sudo ip route add 10.0.0.0/8 via 192.168.1.254
   # Probar que funciona...

2. Persistir con el gestor de red:
   # Si funciona, configurar permanentemente
   nmcli connection modify "eth0" +ipv4.addresses 10.0.0.5/24
   nmcli connection modify "eth0" +ipv4.routes "10.0.0.0/8 192.168.1.254"

3. Verificar:
   # Reiniciar la interfaz y confirmar
   nmcli connection up "eth0"
```

---

## 11. Errores comunes

### Error 1: Usar ifconfig y perder direcciones secundarias

```
INCORRECTO:
$ ifconfig eth0
# Solo muestra la primera dirección IP

# Si tienes 3 IPs en eth0, ifconfig solo muestra 1
# (a menos que uses alias eth0:0, eth0:1)

CORRECTO:
$ ip addr show eth0
# Muestra TODAS las direcciones de la interfaz

# ifconfig fue diseñado cuando una interfaz = una IP.
# El modelo moderno del kernel permite múltiples IPs
# por interfaz, y solo ip addr las muestra todas.
```

### Error 2: Olvidar la máscara al agregar una dirección

```
INCORRECTO:
sudo ip addr add 192.168.1.50 dev eth0
# Añade 192.168.1.50/32 (host route, sin red)
# El host no puede comunicarse con otros en la subred

CORRECTO:
sudo ip addr add 192.168.1.50/24 dev eth0
# Añade con máscara /24, el kernel crea la ruta de subred automáticamente

# Verificar:
ip route show
# Debe aparecer: 192.168.1.0/24 dev eth0 ...
```

### Error 3: Asumir que ip route add persiste tras reinicio

```
INCORRECTO:
sudo ip route add 10.0.0.0/8 via 192.168.1.254
# "Ya configuré la ruta, listo"
# → Desaparece al reiniciar

CORRECTO:
# 1. Configurar temporalmente para probar
sudo ip route add 10.0.0.0/8 via 192.168.1.254

# 2. Si funciona, persistir en el gestor de red
# NetworkManager:
nmcli con mod "eth0" +ipv4.routes "10.0.0.0/8 192.168.1.254"
nmcli con up "eth0"

# systemd-networkd:
# En el archivo .network:
# [Route]
# Destination=10.0.0.0/8
# Gateway=192.168.1.254
```

### Error 4: Confundir ip link set down con "quitar la IP"

```
INCORRECTO:
# "Quiero quitar la IP de eth0"
sudo ip link set eth0 down
# Desactiva toda la interfaz (L2), no solo quita la IP.
# La interfaz no puede recibir ni enviar nada.

CORRECTO:
# Para quitar solo la IP (interfaz sigue UP):
sudo ip addr del 192.168.1.100/24 dev eth0

# Para quitar todas las IPs:
sudo ip addr flush dev eth0

# La interfaz sigue UP a nivel L2, pero sin dirección L3
```

### Error 5: No usar ip route get para debugging

```
INCORRECTO:
# "No sé por qué los paquetes no llegan a 10.0.0.5"
ip route show
# Mirar la tabla de rutas manualmente...
# Hay 30 rutas... ¿cuál se aplica?

CORRECTO:
ip route get 10.0.0.5
# Te dice EXACTAMENTE qué ruta, interfaz y gateway
# usará el kernel para ese destino específico:
# 10.0.0.5 via 192.168.1.254 dev eth0 src 192.168.1.100

# Si no hay ruta:
# RTNETLINK answers: Network is unreachable
```

---

## 12. Cheatsheet

```
IP LINK (Capa 2 — Interfaces)
──────────────────────────────────────────────
ip link show                  Listar interfaces
ip -br link show              Formato breve
ip link show up               Solo interfaces activas
ip link set eth0 up/down      Activar/desactivar
ip link set eth0 mtu 9000     Cambiar MTU
ip link set eth0 address MAC  Cambiar MAC
ip link set eth0 promisc on   Modo promiscuo
ip link add name X type Y     Crear interfaz virtual
ip link del X                 Eliminar interfaz virtual

IP ADDR (Capa 3 — Direcciones)
──────────────────────────────────────────────
ip addr show                  Todas las IPs
ip -br addr show              Formato breve
ip -4 addr show               Solo IPv4
ip -6 addr show               Solo IPv6
ip addr show eth0             IPs de una interfaz
ip addr add IP/M dev eth0     Añadir dirección
ip addr del IP/M dev eth0     Eliminar dirección
ip addr flush dev eth0        Eliminar todas las IPs

IP ROUTE (Tabla de rutas)
──────────────────────────────────────────────
ip route show                 Tabla de rutas
ip route show default         Solo ruta por defecto
ip route get IP               ¿Cómo llego a IP?
ip route add NET via GW       Añadir ruta vía gateway
ip route add default via GW   Ruta por defecto
ip route del NET              Eliminar ruta
ip route replace ...          Crear o reemplazar
ip route add blackhole NET    Descartar paquetes

IP NEIGH (Tabla ARP)
──────────────────────────────────────────────
ip neigh show                 Tabla de vecinos
ip neigh show dev eth0        Vecinos de una interfaz
ip neigh add IP lladdr MAC dev eth0 nud permanent
ip neigh del IP dev eth0      Eliminar entrada
ip neigh flush dev eth0       Limpiar tabla ARP

OPCIONES GLOBALES
──────────────────────────────────────────────
-4              Solo IPv4
-6              Solo IPv6
-s              Estadísticas
-j              JSON
-j -p           JSON legible
-br             Brief (tabla compacta)
-c              Colores
-o              Una línea por registro
ip monitor X    Eventos en tiempo real

EQUIVALENCIAS NET-TOOLS → IPROUTE2
──────────────────────────────────────────────
ifconfig              →  ip addr / ip link
ifconfig eth0 up      →  ip link set eth0 up
ifconfig eth0 IP      →  ip addr add IP dev eth0
route                 →  ip route
route add default gw  →  ip route add default via
arp                   →  ip neigh
netstat               →  ss
```

---

## 13. Ejercicios

### Ejercicio 1: Explorar tu configuración de red actual

Sin modificar nada, obtén la siguiente información de tu sistema:

1. Lista todas las interfaces con `ip -br link show`. ¿Cuáles están UP? ¿Cuáles DOWN?
2. Muestra todas las IPs IPv4 con `ip -4 -br addr show`. Identifica cuáles fueron asignadas por DHCP (`dynamic`) y cuáles son estáticas.
3. Muestra la tabla de rutas. ¿Cuál es tu gateway? ¿Qué interfaz se usa para llegar a Internet?
4. Usa `ip route get 8.8.8.8` y `ip route get 127.0.0.1`. Compara la salida: ¿por qué una tiene `via` y la otra no?
5. Muestra la tabla ARP con `ip neigh show`. ¿Cuántas entradas hay? ¿En qué estado está la entrada de tu gateway?

### Ejercicio 2: Experimentar con direcciones y rutas temporales

En una máquina virtual o contenedor (para no afectar tu red):

1. Añade una segunda IP (10.99.0.1/24) a tu interfaz principal. Verifica con `ip addr show` que ambas direcciones aparecen.
2. Haz ping a 10.99.0.1 desde la propia máquina. ¿Funciona? ¿Por qué?
3. Añade una ruta blackhole para 10.99.99.0/24 y verifica con `ip route get 10.99.99.1` que el paquete se descarta.
4. Elimina la IP y la ruta que añadiste. Verifica que todo volvió al estado original.
5. ¿Qué hubiera pasado si reiniciaras la máquina sin eliminar los cambios?

### Ejercicio 3: Scripting con salida JSON

Escribe un script que, usando `ip -j` y `jq`:

1. Liste todas las interfaces UP con su MAC y su(s) IP(s) IPv4 en formato: `eth0: aa:bb:cc:11:22:33 → 192.168.1.100/24`
2. Muestre el gateway por defecto y por qué interfaz sale.
3. Cuente el número de vecinos ARP en cada estado (REACHABLE, STALE, FAILED).

```bash
# Pistas:
ip -j addr show up | jq -r '...'
ip -j route show default | jq -r '...'
ip -j neigh show | jq -r '...'
```

**Pregunta de reflexión**: Si ejecutas `sudo ip addr add 192.168.1.200/24 dev eth0` en una máquina que ya tiene 192.168.1.100/24 en eth0, ¿qué IP usará el kernel como origen al hacer ping a 192.168.1.50? ¿Y al hacer ping a 10.0.0.1 (otra subred)? Investiga el concepto de "source address selection" y verifica tu respuesta con `ip route get`.

---

> **Siguiente tema**: T02 — NetworkManager (nmcli, nmtui, connection profiles)
