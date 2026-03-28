# Direcciones IPv4

## Índice

1. [Contexto: por qué necesitas entender IPs para virtualización](#1-contexto-por-qué-necesitas-entender-ips-para-virtualización)
2. [Qué es una dirección IP](#2-qué-es-una-dirección-ip)
3. [Anatomía de una IPv4: octetos y bits](#3-anatomía-de-una-ipv4-octetos-y-bits)
4. [Conversión decimal ↔ binario](#4-conversión-decimal--binario)
5. [Tipos de comunicación IP](#5-tipos-de-comunicación-ip)
6. [Direcciones especiales](#6-direcciones-especiales)
7. [Interfaces de red en Linux](#7-interfaces-de-red-en-linux)
8. [Comandos para inspeccionar tu IP](#8-comandos-para-inspeccionar-tu-ip)
9. [Desglose completo de `ip addr`](#9-desglose-completo-de-ip-addr)
10. [IPv4 en el contexto de QEMU/KVM](#10-ipv4-en-el-contexto-de-qemukvm)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Contexto: por qué necesitas entender IPs para virtualización

Cuando lanzas una máquina virtual con QEMU/KVM, esa VM necesita comunicarse con el mundo: con el host, con otras VMs, y posiblemente con internet. Toda esa comunicación ocurre a través de direcciones IP.

Sin entender IPv4, no podrás:

- Diagnosticar por qué una VM no tiene red (`virsh console` muestra la VM pero no puedes hacer `ping`).
- Entender qué hace la red `default` de libvirt (192.168.122.0/24).
- Configurar redes aisladas para laboratorios.
- Hacer port forwarding para acceder a servicios de la VM desde el host.
- Depurar problemas de conectividad entre VMs.

Este tópico cubre lo justo y necesario: no es un curso de redes — es el conocimiento que necesitas para que tus VMs se comuniquen.

---

## 2. Qué es una dirección IP

Una dirección IP (Internet Protocol) es un **identificador numérico único** asignado a cada dispositivo (o interfaz de red) conectado a una red. Es el equivalente a una dirección postal: sin ella, los paquetes de datos no saben a dónde ir.

### Ubicación en el modelo OSI

Las IPs operan en la **capa 3** (Red) del modelo OSI:

```
Capa 7 — Aplicación    (HTTP, SSH, DNS)
Capa 6 — Presentación  (TLS, cifrado)
Capa 5 — Sesión        (gestión de conexiones)
Capa 4 — Transporte    (TCP, UDP — puertos)
Capa 3 — Red           (IP — direccionamiento)  ← aquí
Capa 2 — Enlace        (Ethernet, MAC — switches, bridges)
Capa 1 — Física        (cables, señales eléctricas)
```

En la práctica diaria como sysadmin, las capas que más tocarás son:

| Capa | Qué gestionas | Herramientas |
|------|---------------|-------------|
| 3 — Red | Direcciones IP, rutas | `ip addr`, `ip route` |
| 2 — Enlace | Interfaces, bridges, MACs | `ip link`, `bridge` |
| 4 — Transporte | Puertos, firewalls | `ss`, `iptables`/`nftables` |
| 7 — Aplicación | Servicios | `curl`, `dig`, configuración de daemons |

### IPv4 vs IPv6

IPv4 usa direcciones de 32 bits, lo que da un espacio de ~4.300 millones de direcciones. Este espacio se agotó oficialmente en 2011, lo que motivó IPv6 (128 bits, espacio prácticamente infinito). Sin embargo, IPv4 sigue dominando en redes locales, redes de virtualización, y gran parte de internet gracias a NAT.

En este curso trabajamos exclusivamente con IPv4 porque:

- Las redes de libvirt usan IPv4 por defecto (192.168.122.0/24).
- La mayoría de laboratorios y entornos de virtualización funcionan con IPv4.
- Los conceptos de IPv4 se trasladan directamente a IPv6.

---

## 3. Anatomía de una IPv4: octetos y bits

Una dirección IPv4 tiene este formato:

```
  192  .  168  .    1  .  100
  ───     ───      ─      ───
octeto1  octeto2  octeto3  octeto4
```

### Estructura interna

Cada dirección IPv4 es un número de **32 bits** (4 bytes), dividido en 4 **octetos** de 8 bits cada uno. Cada octeto se representa en notación decimal separada por puntos:

```
Dirección:     192.168.1.100
Binario:       11000000 . 10101000 . 00000001 . 01100100
               ────────   ────────   ────────   ────────
               octeto 1   octeto 2   octeto 3   octeto 4
               (8 bits)   (8 bits)   (8 bits)   (8 bits)
                        = 32 bits total
```

### Rango de cada octeto

Como cada octeto tiene 8 bits, su valor va de **0** a **255**:

```
00000000 = 0     (mínimo)
11111111 = 255   (máximo)
```

Esto significa que `192.168.1.256` **no es una dirección válida** (256 no cabe en 8 bits), y `192.168.1.-1` tampoco.

### Espacio total de direcciones

```
2^32 = 4,294,967,296 direcciones posibles
```

Pero no todas son asignables a hosts: muchas están reservadas para propósitos especiales (broadcast, loopback, multicast, redes privadas, etc.).

---

## 4. Conversión decimal ↔ binario

Saber convertir entre decimal y binario es útil para entender máscaras de red (siguiente tópico) y para diagnosticar problemas de subred. No necesitas hacerlo de memoria — necesitas entender el proceso.

### De decimal a binario

Se usa la tabla de potencias de 2 (de izquierda a derecha: 128, 64, 32, 16, 8, 4, 2, 1):

```
Posición:   128   64   32   16    8    4    2    1
```

Para convertir 192:
```
192 ≥ 128? Sí → 1, quedan 192 - 128 = 64
 64 ≥  64? Sí → 1, quedan  64 -  64 =  0
  0 ≥  32? No → 0
  0 ≥  16? No → 0
  0 ≥   8? No → 0
  0 ≥   4? No → 0
  0 ≥   2? No → 0
  0 ≥   1? No → 0

192 = 11000000
```

Para convertir 168:
```
168 ≥ 128? Sí → 1, quedan 168 - 128 = 40
 40 ≥  64? No → 0
 40 ≥  32? Sí → 1, quedan  40 -  32 =  8
  8 ≥  16? No → 0
  8 ≥   8? Sí → 1, quedan   8 -   8 =  0
  0 ≥   4? No → 0
  0 ≥   2? No → 0
  0 ≥   1? No → 0

168 = 10101000
```

### De binario a decimal

Se suman las posiciones donde hay un `1`:

```
10101000
│ │ │
│ │ └─ 8
│ └─── 32
└───── 128

128 + 32 + 8 = 168
```

### Ejemplo completo: 10.0.0.1

```
 10 = 00001010    (8 + 2)
  0 = 00000000
  0 = 00000000
  1 = 00000001    (1)

10.0.0.1 = 00001010.00000000.00000000.00000001
```

### Verificación rápida desde la terminal

```bash
# Decimal a binario (cada octeto por separado)
echo "obase=2; 192" | bc
# 11000000

# Binario a decimal
echo "ibase=2; 11000000" | bc
# 192

# Python one-liner para la IP completa
python3 -c "print('.'.join(f'{int(o):08b}' for o in '192.168.1.100'.split('.')))"
# 11000000.10101000.00000001.01100100
```

---

## 5. Tipos de comunicación IP

IPv4 define tres modos de comunicación según cuántos destinatarios reciben el paquete.

### Unicast: uno a uno

El caso más común. Un paquete viaja desde un origen a **un solo destino** específico:

```
 Host A                           Host B
192.168.1.10 ──── paquete ────→ 192.168.1.20
```

Ejemplos cotidianos:
- Hacer SSH a una VM: `ssh user@192.168.122.100`
- Un navegador accediendo a un servidor web.
- Transferir un archivo con `scp`.

Prácticamente toda la comunicación que harás con VMs es unicast.

### Broadcast: uno a todos

Un paquete se envía a **todos los dispositivos** de la red local. El destinatario es la dirección de broadcast de la subred (típicamente la última dirección del rango):

```
 Host A                    ┌→ Host B (192.168.1.20)
192.168.1.10 ── paquete ──→├→ Host C (192.168.1.30)
      destino:             └→ Host D (192.168.1.40)
   192.168.1.255
   (broadcast)
```

| Red | Dirección de broadcast |
|-----|----------------------|
| 192.168.1.0/24 | 192.168.1.255 |
| 10.0.0.0/8 | 10.255.255.255 |
| 172.16.0.0/16 | 172.16.255.255 |
| 192.168.122.0/24 | 192.168.122.255 |

Los broadcasts **no cruzan routers** — se limitan a la red local (dominio de broadcast). Esto es importante en virtualización: un broadcast en la red `default` de libvirt solo llega a las VMs en esa red, no sale al host ni a internet.

Usos reales de broadcast:
- **DHCP Discover**: una VM nueva envía un broadcast pidiendo "¿hay algún servidor DHCP?".
- **ARP**: "¿quién tiene la IP 192.168.122.1? Dígame su MAC".

### Multicast: uno a muchos (grupo)

Un paquete se envía a un **grupo de dispositivos** que se han suscrito a una dirección multicast (rango 224.0.0.0 – 239.255.255.255):

```
 Host A                    ┌→ Host B (suscrito)
192.168.1.10 ── paquete ──→│  Host C (NO suscrito, no recibe)
      destino:             └→ Host D (suscrito)
   224.0.0.5
   (grupo multicast)
```

Usos:
- Protocolos de routing (OSPF usa 224.0.0.5).
- Streaming de video.
- Descubrimiento de servicios (mDNS usa 224.0.0.251).

Para virtualización, el multicast rara vez es relevante directamente, pero puede aparecer en logs de red.

### Comparación

```
Unicast:    A ───────────────→ B              (1 a 1)
Broadcast:  A ───────────────→ todos en L2    (1 a todos)
Multicast:  A ───────────────→ grupo inscrito (1 a muchos)
```

---

## 6. Direcciones especiales

IPv4 reserva ciertos rangos para propósitos específicos. Conocer estos rangos te evita confusiones al inspeccionar la red de tus VMs.

### 127.0.0.0/8 — Loopback

Todo el rango 127.0.0.0 a 127.255.255.255 es loopback, aunque en la práctica solo se usa `127.0.0.1`. El tráfico enviado a loopback **nunca sale de la máquina** — no toca ninguna interfaz de red física.

```bash
ping -c 2 127.0.0.1
# PING 127.0.0.1 (127.0.0.1) 56(84) bytes of data.
# 64 bytes from 127.0.0.1: icmp_seq=1 ttl=64 time=0.031 ms
# 64 bytes from 127.0.0.1: icmp_seq=2 ttl=64 time=0.045 ms
```

La latencia de ~0.03ms confirma que el paquete no salió de la máquina. Si ves latencia alta en loopback, algo está muy mal en el sistema.

Usos:
- Probar que un servicio está escuchando: `curl http://127.0.0.1:8080`.
- La VM tiene su propio loopback — si haces `ping 127.0.0.1` dentro de la VM, es el loopback de la VM, no del host.

En IPv6, el equivalente es `::1`.

### 0.0.0.0 — "cualquier dirección" o "esta red"

El significado depende del contexto:

| Contexto | Significado |
|----------|------------|
| Un servidor escucha en `0.0.0.0:80` | Acepta conexiones en **todas** las interfaces |
| Ruta por defecto `0.0.0.0/0` | Cualquier destino (ruta catch-all) |
| Origen `0.0.0.0` en DHCP | "Aún no tengo IP" |

Cuando un servicio escucha en `0.0.0.0`, acepta conexiones tanto desde loopback como desde cualquier interfaz de red. Si ves esto en un servicio de una VM, significa que es accesible desde fuera de la VM (si la red lo permite).

```bash
# Servicio escuchando en 0.0.0.0 (todas las interfaces)
ss -tlnp | grep 80
# LISTEN  0  128  0.0.0.0:80  0.0.0.0:*  users:(("nginx",pid=1234))

# vs servicio escuchando solo en loopback (solo local)
ss -tlnp | grep 5432
# LISTEN  0  128  127.0.0.1:5432  0.0.0.0:*  users:(("postgres",pid=5678))
```

### 169.254.0.0/16 — Link-local (APIPA)

Si un dispositivo no puede obtener IP por DHCP, se auto-asigna una dirección en el rango 169.254.x.x. **Si ves esta IP en tu VM, significa que DHCP falló**:

```bash
ip addr show eth0
# inet 169.254.x.x/16   ← problema: DHCP no respondió
```

Diagnóstico:
1. ¿La red virtual está activa? `virsh net-list --all`
2. ¿La VM está conectada a la red correcta? `virsh domiflist <vm>`
3. ¿El servicio DHCP de libvirt funciona? `systemctl status libvirtd`

### 255.255.255.255 — Broadcast limitado

Broadcast a todos los dispositivos de la red local sin importar la subred. Usado internamente por DHCP.

### Resumen de rangos especiales

| Rango | Nombre | Propósito |
|-------|--------|-----------|
| 0.0.0.0/8 | "Esta red" | Dirección por defecto, binding |
| 10.0.0.0/8 | Privada clase A | Redes privadas grandes |
| 127.0.0.0/8 | Loopback | Comunicación consigo mismo |
| 169.254.0.0/16 | Link-local | Auto-asignación sin DHCP |
| 172.16.0.0/12 | Privada clase B | Redes privadas medianas |
| 192.168.0.0/16 | Privada clase C | Redes privadas pequeñas |
| 224.0.0.0/4 | Multicast | Comunicación a grupos |
| 255.255.255.255/32 | Broadcast | Broadcast limitado |

Las direcciones privadas se cubren en detalle en T03.

---

## 7. Interfaces de red en Linux

Antes de inspeccionar IPs, necesitas entender qué son las interfaces de red. Una interfaz es un **punto de conexión a una red** — puede ser física (tarjeta de red) o virtual (creada por software).

### Naming de interfaces

Linux usa nombres predecibles desde systemd (Predictable Network Interface Names). El esquema de nombres depende de cómo se detecta el hardware:

| Prefijo | Significado | Ejemplo | Dónde lo verás |
|---------|------------|---------|----------------|
| `en` | Ethernet | `enp0s3`, `ens33` | Tarjetas de red físicas o emuladas |
| `wl` | Wireless LAN | `wlp2s0` | WiFi |
| `lo` | Loopback | `lo` | Siempre presente, `127.0.0.1` |
| `virbr` | Virtual bridge | `virbr0` | Creado por libvirt (red NAT) |
| `vnet` | Virtual NIC | `vnet0`, `vnet1` | TAP device conectando VM al bridge |
| `veth` | Virtual Ethernet pair | `veth1234` | Contenedores (Docker) |
| `br` | Bridge | `br0` | Bridges manuales |
| `docker` | Docker bridge | `docker0` | Bridge de Docker |

### Naming predecible: decodificando el nombre

El nombre `enp0s3` codifica información:

```
en   p0   s3
──   ──   ──
│    │    └── slot 3 (tercer slot PCI)
│    └─────── bus 0 (bus PCI 0)
└──────────── ethernet
```

En una VM típica de VirtualBox verás `enp0s3`. En VMware suele ser `ens33`. En QEMU/KVM con virtio puede ser `enp1s0` o similar.

### Interfaces que libvirt crea

Cuando instalas libvirt y activas la red `default`, se crea automáticamente:

```bash
ip link show type bridge
# virbr0: <NO-CARRIER,BROADCAST,MULTICAST,UP> mtu 1500
#     link/ether 52:54:00:xx:xx:xx

ip addr show virbr0
# inet 192.168.122.1/24 brd 192.168.122.255 scope global virbr0
```

- **virbr0**: bridge virtual. Es el "switch" al que se conectan las VMs con red NAT.
- **virbr0-nic**: interfaz dummy conectada al bridge (para que tenga una MAC).
- **vnetN**: interfaz TAP creada **por cada VM en ejecución**, conectada al bridge.

Cuando arrancas una VM:
```
VM (eth0) ←──── vnet0 ──── virbr0 (192.168.122.1) ──── host ──── internet
                             │
VM (eth0) ←──── vnet1 ──────┘
```

---

## 8. Comandos para inspeccionar tu IP

### ip addr (forma canónica)

```bash
ip addr
# o la forma abreviada:
ip a
```

Este es el comando estándar del paquete `iproute2`. Reemplaza al antiguo `ifconfig`.

### Variantes útiles

```bash
# Solo IPv4 (excluye IPv6)
ip -4 addr

# Solo una interfaz específica
ip addr show enp0s3

# Formato compacto (una línea por interfaz)
ip -brief addr
# lo        UNKNOWN  127.0.0.1/8 ::1/128
# enp0s3    UP       192.168.1.50/24
# virbr0    UP       192.168.122.1/24

# Solo interfaces UP
ip -brief addr show up

# Con colores (más legible)
ip -c addr

# Solo la IP sin ruido (scriptable)
hostname -I
# 192.168.1.50 192.168.122.1

# Forma clásica (deprecated pero funcional)
ifconfig
```

### Cuándo usar cada variante

| Necesitas | Comando |
|-----------|---------|
| Inspección completa | `ip addr` |
| Vista rápida de todas las IPs | `ip -brief addr` |
| Solo IPv4 | `ip -4 addr` |
| Solo una interfaz | `ip addr show <nombre>` |
| IPs para un script | `hostname -I` |
| Dentro de una VM mínima sin iproute2 | `cat /proc/net/fib_trie` (último recurso) |

---

## 9. Desglose completo de `ip addr`

Entender la salida de `ip addr` es una habilidad clave. Vamos línea por línea:

```
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN group default qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    inet 127.0.0.1/8 scope host lo
       valid_lft forever preferred_lft forever
    inet6 ::1/128 scope host noprefixroute
       valid_lft forever preferred_lft forever

2: enp0s3: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP group default qlen 1000
    link/ether 08:00:27:ab:cd:ef brd ff:ff:ff:ff:ff:ff
    inet 192.168.1.50/24 brd 192.168.1.255 scope global dynamic noprefixroute enp0s3
       valid_lft 85432sec preferred_lft 85432sec
    inet6 fe80::a00:27ff:feab:cdef/64 scope link noprefixroute
       valid_lft forever preferred_lft forever

5: virbr0: <NO-CARRIER,BROADCAST,MULTICAST,UP> mtu 1500 qdisc noqueue state DOWN group default qlen 1000
    link/ether 52:54:00:12:34:56 brd ff:ff:ff:ff:ff:ff
    inet 192.168.122.1/24 brd 192.168.122.255 scope global virbr0
       valid_lft forever preferred_lft forever
```

### Línea 1: flags y estado de la interfaz

```
2: enp0s3: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP
│  │        │                                 │        │              │
│  │        │                                 │        │              └── state: UP = activa
│  │        │                                 │        └── qdisc: algoritmo de encolamiento
│  │        │                                 └── MTU: tamaño máximo de paquete (bytes)
│  │        └── flags entre < >
│  └── nombre de la interfaz
└── índice numérico
```

**Flags importantes:**

| Flag | Significado |
|------|------------|
| `UP` | La interfaz está habilitada a nivel de software |
| `LOWER_UP` | Hay enlace físico (cable conectado, o VM activa) |
| `NO-CARRIER` | No hay enlace físico (sin cable, o ninguna VM conectada) |
| `BROADCAST` | Soporta broadcast |
| `MULTICAST` | Soporta multicast |
| `LOOPBACK` | Es la interfaz de loopback |

**Estado `NO-CARRIER` en virbr0**: cuando no hay VMs ejecutándose, virbr0 muestra `NO-CARRIER` y `state DOWN`. Esto es normal — el bridge no tiene a nadie conectado. Cuando arrancas una VM, aparece `LOWER_UP` y `state UP`.

### Línea 2: dirección de capa 2 (MAC)

```
    link/ether 08:00:27:ab:cd:ef brd ff:ff:ff:ff:ff:ff
               ─────────────────     ─────────────────
               MAC de la interfaz    MAC de broadcast
```

- **MAC address**: identificador de 48 bits de la interfaz de red (capa 2). Único por interfaz.
- `08:00:27:xx:xx:xx`: prefijo de VirtualBox. `52:54:00:xx:xx:xx`: prefijo de QEMU/KVM.
- `ff:ff:ff:ff:ff:ff`: dirección de broadcast de capa 2 (broadcast Ethernet).

### Línea 3: dirección IP (capa 3)

```
    inet 192.168.1.50/24 brd 192.168.1.255 scope global dynamic noprefixroute enp0s3
         ───────────────     ─────────────       ──────  ───────
         IP/máscara          broadcast            scope   origen
```

| Campo | Significado |
|-------|------------|
| `inet` | Dirección IPv4 (vs `inet6` para IPv6) |
| `192.168.1.50/24` | IP con máscara CIDR |
| `brd 192.168.1.255` | Dirección de broadcast de la subred |
| `scope global` | Dirección ruteable (vs `scope link` = solo local, `scope host` = solo loopback) |
| `dynamic` | Asignada por DHCP (se renueva) |
| `noprefixroute` | No crea ruta automática para el prefijo |

### Línea 4: tiempos de vida del lease

```
       valid_lft 85432sec preferred_lft 85432sec
```

- **valid_lft**: cuánto tiempo queda antes de que la IP expire (DHCP lease).
- **preferred_lft**: cuánto tiempo la IP se prefiere para nuevas conexiones.
- Si ves `forever`, la IP es estática (configurada manualmente o es la del propio bridge).

### Diagnóstico rápido con la salida

| Lo que ves | Significado | Acción |
|-----------|------------|--------|
| Sin línea `inet` en una interfaz | No tiene IP asignada | Verificar DHCP o configuración estática |
| `state DOWN` | Interfaz deshabilitada | `ip link set <if> up` |
| `NO-CARRIER` | Sin enlace físico | Verificar cable/VM conectada |
| `scope link` en inet | IP link-local (169.254.x.x) | DHCP falló — revisar la red |
| `dynamic` | Viene de DHCP | Normal en hosts y VMs con DHCP |
| `valid_lft` bajo | El lease DHCP expira pronto | Se renueva automáticamente; si falla, la IP se pierde |

---

## 10. IPv4 en el contexto de QEMU/KVM

Al trabajar con libvirt/QEMU/KVM, encontrarás estas IPs recurrentemente:

### La red default de libvirt

```
Red:        192.168.122.0/24
Gateway:    192.168.122.1  (virbr0 en el host)
DHCP:       192.168.122.2 – 192.168.122.254
Broadcast:  192.168.122.255
```

El host actúa como gateway y servidor DHCP/DNS para las VMs:

```
┌─────────────────────────────────────────┐
│            HOST (tu máquina)            │
│                                         │
│  enp0s3: 192.168.1.50                   │
│      │                                  │
│      │   virbr0: 192.168.122.1          │
│      │      │         │                 │
│      │   vnet0     vnet1                │
│    [NAT]    │         │                 │
│      │   ┌──┴──┐   ┌──┴──┐             │
│      │   │ VM1 │   │ VM2 │             │
│      │   │ .100│   │ .101│             │
│      │   └─────┘   └─────┘             │
│      │                                  │
│   internet                              │
└─────────────────────────────────────────┘
```

### IPs que verás frecuentemente

| IP | Dónde/qué es |
|----|-------------|
| 192.168.1.x | Tu máquina en la LAN doméstica |
| 192.168.122.1 | Host visto desde las VMs (gateway) |
| 192.168.122.100+ | VMs con DHCP de libvirt |
| 127.0.0.1 | Loopback (tanto en host como en cada VM) |
| 10.0.2.15 | IP por defecto en QEMU user-mode networking (sin libvirt) |

### Ver la IP de una VM sin entrar en ella

```bash
# Desde el host, ver los leases DHCP de la red default
virsh net-dhcp-leases default
# Expiry               MAC               Protocol  IP address       Hostname
# 2024-01-15 10:30:00  52:54:00:ab:cd:ef ipv4      192.168.122.100  fedora-vm

# También con arp scan (requiere nmap)
nmap -sn 192.168.122.0/24
```

---

## 11. Errores comunes

### Error 1: Confundir la IP del host con la IP del bridge

```bash
# En el host:
ip -brief addr
# enp0s3    UP    192.168.1.50/24       ← IP del host en la LAN
# virbr0    UP    192.168.122.1/24      ← IP del host en la red de VMs

# Desde una VM, el host se ve como 192.168.122.1, NO como 192.168.1.50
# Si haces ping 192.168.1.50 desde la VM, funciona (NAT)
# Pero 192.168.122.1 es la puerta de enlace directa
```

El host tiene múltiples IPs — una por cada interfaz. El bridge virbr0 es una interfaz más del host con su propia IP en una red diferente.

### Error 2: Ver 169.254.x.x y pensar que funciona

```bash
# Dentro de la VM:
ip addr show eth0
# inet 169.254.45.67/16 scope link
```

Una IP 169.254.x.x (link-local) significa que **DHCP no respondió**. La VM se auto-asignó una IP temporal que solo permite comunicación en la misma red local (y ni siquiera siempre). No tendrá acceso a internet ni al host.

**Diagnóstico:**
```bash
# 1. ¿La red virtual está activa?
virsh net-list --all
# Si state es "inactive": virsh net-start default

# 2. ¿La VM está conectada a esa red?
virsh domiflist <nombre-vm>

# 3. ¿libvirtd está corriendo?
systemctl status libvirtd
```

### Error 3: Usar ifconfig en lugar de ip addr

`ifconfig` es del paquete `net-tools`, deprecated desde hace más de una década. No muestra todas las IPs secundarias de una interfaz y no existe en instalaciones mínimas modernas.

```bash
# Mal (puede no estar instalado, muestra info incompleta):
ifconfig

# Bien:
ip addr

# O para vista rápida:
ip -brief addr
```

### Error 4: Creer que ip addr "configura" la red

`ip addr` **solo muestra** información. Para asignar o modificar una IP:

```bash
# Asignar IP temporal (se pierde al reiniciar):
sudo ip addr add 192.168.1.200/24 dev enp0s3

# Eliminar IP:
sudo ip addr del 192.168.1.200/24 dev enp0s3

# Para configuración permanente, usa NetworkManager o /etc/network/interfaces
```

### Error 5: Esperar que la VM tenga IP inmediatamente al arrancar

Después de `virsh start <vm>`, la VM necesita:
1. Arrancar el kernel.
2. Cargar los drivers de red.
3. Enviar un DHCP Discover (broadcast).
4. Recibir y procesar el lease.

Esto puede tardar entre 5 y 30 segundos. Si consultas `virsh net-dhcp-leases default` demasiado pronto, la VM aún no aparecerá.

---

## 12. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                    DIRECCIONES IPv4 CHEATSHEET                      ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  ESTRUCTURA                                                        ║
║  4 octetos × 8 bits = 32 bits total                                ║
║  Cada octeto: 0–255 (decimal) = 00000000–11111111 (binario)        ║
║  Espacio total: 2^32 = ~4.3 mil millones de direcciones            ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  TIPOS DE COMUNICACIÓN                                             ║
║  Unicast      1 → 1        (SSH, HTTP, la mayoría del tráfico)     ║
║  Broadcast    1 → todos    (DHCP discover, ARP)                    ║
║  Multicast    1 → grupo    (OSPF, mDNS, streaming)                 ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  DIRECCIONES ESPECIALES                                            ║
║  127.0.0.1         Loopback (localhost)                             ║
║  0.0.0.0           "Cualquier interfaz" / sin IP aún               ║
║  169.254.x.x       Link-local (DHCP falló)                         ║
║  255.255.255.255   Broadcast limitado                               ║
║  192.168.122.1     Gateway de libvirt (virbr0)                     ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  INTERFACES COMUNES                                                ║
║  lo            Loopback                                             ║
║  enp0s3        Ethernet (VirtualBox)                                ║
║  ens33         Ethernet (VMware)                                    ║
║  virbr0        Bridge NAT de libvirt                                ║
║  vnetN         TAP de VM conectada al bridge                        ║
║  docker0       Bridge de Docker                                     ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  COMANDOS                                                          ║
║  ip addr                     Mostrar todas las interfaces e IPs    ║
║  ip -4 addr                  Solo IPv4                              ║
║  ip -brief addr              Vista compacta (1 línea/interfaz)     ║
║  ip addr show <iface>        Una interfaz específica                ║
║  ip -c addr                  Con colores                            ║
║  hostname -I                 Solo IPs (para scripts)                ║
║  virsh net-dhcp-leases X     IPs asignadas por DHCP de libvirt     ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  DIAGNÓSTICO RÁPIDO                                                ║
║  Sin inet → no tiene IP (revisar DHCP/config)                      ║
║  169.254.x.x → DHCP falló                                          ║
║  NO-CARRIER → sin enlace físico (cable/VM desconectada)            ║
║  state DOWN → interfaz deshabilitada                                ║
║  scope link → IP solo válida en el enlace local                    ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 13. Ejercicios

### Ejercicio 1: Inspección de interfaces de red

Ejecuta los siguientes comandos y responde las preguntas:

```bash
ip -brief addr
ip addr show lo
ip addr show <tu-interfaz-ethernet>
```

**Preguntas:**
1. ¿Cuántas interfaces de red tiene tu máquina (sin contar `lo`)?
2. ¿Tu IP fue asignada por DHCP o es estática? (pista: busca `dynamic` en la salida de `ip addr`).
3. ¿Cuál es la dirección de broadcast de tu subred?
4. Si tienes libvirt instalado, ¿aparece `virbr0`? ¿Qué IP tiene? ¿Su estado es UP o DOWN? ¿Por qué?
5. ¿Cuál es la MAC address de tu interfaz ethernet? ¿Qué fabricante tiene según el prefijo? (busca los 3 primeros bytes del OUI).

### Ejercicio 2: Conversión binario ↔ decimal

Convierte sin ayuda de herramientas (luego verifica con `echo "obase=2; X" | bc`):

**De decimal a binario:**
```
a) 10.0.0.1        →  ¿?
b) 172.16.0.1       →  ¿?
c) 192.168.122.1    →  ¿?
```

**De binario a decimal:**
```
d) 11111111.00000000.00000000.00000000  →  ¿?
e) 10101100.00010000.11111110.00000001  →  ¿?
f) 11000000.10101000.01111010.00000001  →  ¿?
```

**Pregunta reflexiva:** La dirección del ejercicio (d) es 255.0.0.0. ¿Es una dirección IP válida para asignar a un host? ¿Qué función cumple ese patrón de "todos unos" en los primeros bits?

### Ejercicio 3: Diagnóstico de red en un escenario de VM

Imagina que arrancaste una VM con `virsh start fedora-vm` y quieres conectarte por SSH, pero no sabes su IP.

**Describe los pasos en orden:**
1. ¿Qué comando usarías para ver si la VM obtuvo IP por DHCP?
2. Si el comando anterior no devuelve nada, ¿qué verificarías primero?
3. Si la VM tiene IP 169.254.x.x, ¿qué significa y cómo lo arreglas?
4. Si la VM tiene 192.168.122.105, ¿qué comando usas para conectarte por SSH?
5. Una vez conectado, ejecutas `ip addr` dentro de la VM. ¿Esperarías ver `virbr0`? ¿Por qué sí o no?

**Pregunta extra:** Si otra persona en tu red local (192.168.1.0/24) quisiera acceder directamente a tu VM (192.168.122.105), ¿podría? ¿Por qué?

---

> **Nota**: una dirección IPv4 es simplemente un número de 32 bits. Toda la complejidad de redes — subredes, NAT, routing, firewalls — se construye sobre la interpretación de esos 32 bits. Al dominar cómo se estructura una IP y cómo inspeccionarla con `ip addr`, tienes la base para entender todo lo que viene: máscaras de red, DHCP, NAT, y redes virtuales en libvirt.
