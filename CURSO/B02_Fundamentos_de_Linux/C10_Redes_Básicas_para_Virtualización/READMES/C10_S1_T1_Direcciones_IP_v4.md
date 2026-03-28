# Direcciones IPv4

> **Objetivo:** Entender la estructura de las direcciones IPv4, los tipos de comunicación, las direcciones especiales, las interfaces de red en Linux, y cómo inspeccionar IPs con `ip addr` — todo enfocado en lo necesario para trabajar con virtualización (QEMU/KVM/libvirt).

> Sin erratas detectadas en el material original.

---

## 1. Contexto: por qué necesitas entender IPs para virtualización

Cuando lanzas una máquina virtual con QEMU/KVM, esa VM necesita comunicarse con el mundo: con el host, con otras VMs, y posiblemente con internet. Toda esa comunicación ocurre a través de direcciones IP.

Sin entender IPv4, no podrás:

- Diagnosticar por qué una VM no tiene red (`virsh console` muestra la VM pero no puedes hacer `ping`).
- Entender qué hace la red `default` de libvirt (192.168.122.0/24).
- Configurar redes aisladas para laboratorios.
- Hacer port forwarding para acceder a servicios de la VM desde el host.
- Depurar problemas de conectividad entre VMs.

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

IPv4 usa direcciones de 32 bits (~4.300 millones de direcciones). Este espacio se agotó oficialmente en 2011, lo que motivó IPv6 (128 bits, espacio prácticamente infinito). Sin embargo, IPv4 sigue dominando en redes locales, redes de virtualización, y gran parte de internet gracias a NAT.

En este curso trabajamos exclusivamente con IPv4 porque:

- Las redes de libvirt usan IPv4 por defecto (192.168.122.0/24).
- La mayoría de laboratorios y entornos de virtualización funcionan con IPv4.
- Los conceptos de IPv4 se trasladan directamente a IPv6.

---

## 3. Anatomía de una IPv4: octetos y bits

Una dirección IPv4 es un número de **32 bits** (4 bytes), dividido en 4 **octetos** de 8 bits cada uno. Cada octeto se representa en notación decimal separada por puntos:

```
Dirección:     192.168.1.100
Binario:       11000000 . 10101000 . 00000001 . 01100100
               ────────   ────────   ────────   ────────
               octeto 1   octeto 2   octeto 3   octeto 4
               (8 bits)   (8 bits)   (8 bits)   (8 bits)
                        = 32 bits total
```

### Rango de cada octeto

Cada octeto tiene 8 bits, su valor va de **0** a **255**:

```
00000000 = 0     (mínimo)
11111111 = 255   (máximo)
```

Esto significa que `192.168.1.256` **no es una dirección válida** (256 no cabe en 8 bits), y `192.168.1.-1` tampoco.

### Espacio total de direcciones

```
2^32 = 4,294,967,296 direcciones posibles
```

Pero no todas son asignables a hosts: muchas están reservadas (broadcast, loopback, multicast, redes privadas, etc.).

---

## 4. Conversión decimal ↔ binario

Saber convertir entre decimal y binario es útil para entender máscaras de red (siguiente tópico) y para diagnosticar problemas de subred. No necesitas hacerlo de memoria — necesitas entender el proceso.

### De decimal a binario

Se usa la tabla de potencias de 2 (de izquierda a derecha):

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

Ejemplos cotidianos: SSH a una VM, navegador accediendo a un servidor web, transferir un archivo con `scp`. Prácticamente toda la comunicación con VMs es unicast.

### Broadcast: uno a todos

Un paquete se envía a **todos los dispositivos** de la red local. El destinatario es la dirección de broadcast de la subred (la última dirección del rango):

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

Los broadcasts **no cruzan routers** — se limitan a la red local (dominio de broadcast). En virtualización: un broadcast en la red `default` de libvirt solo llega a las VMs en esa red, no sale al host ni a internet.

Usos reales de broadcast:
- **DHCP Discover**: una VM nueva envía un broadcast pidiendo "¿hay algún servidor DHCP?".
- **ARP**: "¿quién tiene la IP 192.168.122.1? Dígame su MAC".

### Multicast: uno a muchos (grupo)

Un paquete se envía a un **grupo de dispositivos** suscritos a una dirección multicast (rango 224.0.0.0 – 239.255.255.255):

```
 Host A                    ┌→ Host B (suscrito)
192.168.1.10 ── paquete ──→│  Host C (NO suscrito, no recibe)
      destino:             └→ Host D (suscrito)
   224.0.0.5
   (grupo multicast)
```

Usos: protocolos de routing (OSPF usa 224.0.0.5), streaming de video, descubrimiento de servicios (mDNS usa 224.0.0.251). Para virtualización, el multicast rara vez es relevante, pero puede aparecer en logs de red.

### Comparación

```
Unicast:    A ───────────────→ B              (1 a 1)
Broadcast:  A ───────────────→ todos en L2    (1 a todos)
Multicast:  A ───────────────→ grupo inscrito (1 a muchos)
```

---

## 6. Direcciones especiales

IPv4 reserva ciertos rangos para propósitos específicos:

| Rango | Nombre | Propósito |
|-------|--------|-----------|
| 0.0.0.0/8 | "Esta red" | Dirección por defecto, binding |
| 10.0.0.0/8 | Privada clase A | Redes privadas grandes |
| 127.0.0.0/8 | Loopback | Comunicación consigo mismo |
| 169.254.0.0/16 | Link-local (APIPA) | Auto-asignación sin DHCP |
| 172.16.0.0/12 | Privada clase B | Redes privadas medianas |
| 192.168.0.0/16 | Privada clase C | Redes privadas pequeñas |
| 224.0.0.0/4 | Multicast | Comunicación a grupos |
| 255.255.255.255/32 | Broadcast | Broadcast limitado |

### 127.0.0.1 — Loopback

Todo el rango 127.0.0.0/8 es loopback, aunque en la práctica solo se usa `127.0.0.1`. El tráfico **nunca sale de la máquina** — no toca ninguna interfaz de red física.

```bash
ping -c 2 127.0.0.1
# 64 bytes from 127.0.0.1: icmp_seq=1 ttl=64 time=0.031 ms
```

La latencia de ~0.03ms confirma que el paquete no salió de la máquina. La VM tiene su propio loopback — `ping 127.0.0.1` dentro de la VM es el loopback de la VM, no del host.

### 0.0.0.0 — "cualquier dirección"

| Contexto | Significado |
|----------|------------|
| Servidor escucha en `0.0.0.0:80` | Acepta conexiones en **todas** las interfaces |
| Ruta por defecto `0.0.0.0/0` | Cualquier destino (ruta catch-all) |
| Origen `0.0.0.0` en DHCP | "Aún no tengo IP" |

```bash
# Servicio en 0.0.0.0 (todas las interfaces) — accesible desde fuera:
ss -tlnp | grep 80
# LISTEN  0  128  0.0.0.0:80  0.0.0.0:*  users:(("nginx",pid=1234))

# vs servicio en 127.0.0.1 (solo local) — no accesible desde fuera:
ss -tlnp | grep 5432
# LISTEN  0  128  127.0.0.1:5432  0.0.0.0:*  users:(("postgres",pid=5678))
```

### 169.254.x.x — Link-local (APIPA)

Si un dispositivo no puede obtener IP por DHCP, se auto-asigna una dirección 169.254.x.x. **Si ves esta IP en tu VM, significa que DHCP falló**:

```bash
ip addr show eth0
# inet 169.254.x.x/16   ← problema: DHCP no respondió
```

Diagnóstico:
1. ¿La red virtual está activa? `virsh net-list --all`
2. ¿La VM está conectada a la red correcta? `virsh domiflist <vm>`
3. ¿El servicio DHCP de libvirt funciona? `systemctl status libvirtd`

---

## 7. Interfaces de red en Linux

Una interfaz es un **punto de conexión a una red** — puede ser física (tarjeta de red) o virtual (creada por software).

### Naming predecible (systemd)

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

### Decodificando el nombre

```
en   p0   s3
──   ──   ──
│    │    └── slot PCI 3
│    └─────── bus PCI 0
└──────────── ethernet
```

En una VM de VirtualBox verás `enp0s3`. En VMware suele ser `ens33`. En QEMU/KVM con virtio puede ser `enp1s0` o similar.

### Interfaces que libvirt crea

Cuando instalas libvirt y activas la red `default`:

- **virbr0**: bridge virtual. Es el "switch" al que se conectan las VMs con red NAT. IP: 192.168.122.1/24.
- **vnetN**: interfaz TAP creada **por cada VM en ejecución**, conectada al bridge.

```
VM (eth0) ←──── vnet0 ──── virbr0 (192.168.122.1) ──── host ──── internet
                             │
VM (eth0) ←──── vnet1 ──────┘
```

---

## 8. Comandos para inspeccionar tu IP

### ip addr (forma canónica)

```bash
ip addr    # completo
ip a       # abreviado
```

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

| Necesitas | Comando |
|-----------|---------|
| Inspección completa | `ip addr` |
| Vista rápida de todas las IPs | `ip -brief addr` |
| Solo IPv4 | `ip -4 addr` |
| Solo una interfaz | `ip addr show <nombre>` |
| IPs para un script | `hostname -I` |
| Dentro de VM mínima sin iproute2 | `cat /proc/net/fib_trie` (último recurso) |

---

## 9. Desglose completo de `ip addr`

```
2: enp0s3: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP
    link/ether 08:00:27:ab:cd:ef brd ff:ff:ff:ff:ff:ff
    inet 192.168.1.50/24 brd 192.168.1.255 scope global dynamic noprefixroute enp0s3
       valid_lft 85432sec preferred_lft 85432sec
```

### Línea 1: flags y estado

```
2: enp0s3: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP
│  │        │                                 │        │              │
│  │        │                                 │        │              └── state: UP = activa
│  │        │                                 │        └── qdisc: algoritmo de encolamiento
│  │        │                                 └── MTU: tamaño máximo de paquete
│  │        └── flags entre < >
│  └── nombre de la interfaz
└── índice numérico
```

**Flags importantes:**

| Flag | Significado |
|------|------------|
| `UP` | Interfaz habilitada a nivel de software |
| `LOWER_UP` | Hay enlace físico (cable conectado, o VM activa) |
| `NO-CARRIER` | No hay enlace físico (sin cable, o ninguna VM conectada) |
| `BROADCAST` | Soporta broadcast |
| `MULTICAST` | Soporta multicast |
| `LOOPBACK` | Es la interfaz de loopback |

**`NO-CARRIER` en virbr0**: cuando no hay VMs ejecutándose, virbr0 muestra `NO-CARRIER` y `state DOWN`. Es normal — el bridge no tiene a nadie conectado. Al arrancar una VM, cambia a `LOWER_UP` y `state UP`.

### Línea 2: dirección MAC (capa 2)

```
    link/ether 08:00:27:ab:cd:ef brd ff:ff:ff:ff:ff:ff
               ─────────────────     ─────────────────
               MAC de la interfaz    MAC de broadcast
```

- `08:00:27:xx:xx:xx`: prefijo OUI de VirtualBox.
- `52:54:00:xx:xx:xx`: prefijo OUI de QEMU/KVM.
- `ff:ff:ff:ff:ff:ff`: broadcast Ethernet (capa 2).

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

### Línea 4: tiempos de vida (DHCP lease)

```
       valid_lft 85432sec preferred_lft 85432sec
```

- **valid_lft**: tiempo restante antes de que la IP expire.
- **preferred_lft**: tiempo restante como IP preferida para nuevas conexiones.
- Si ves `forever`, la IP es estática o es la del propio bridge.

### Diagnóstico rápido

| Lo que ves | Significado | Acción |
|-----------|------------|--------|
| Sin línea `inet` | No tiene IP | Verificar DHCP o config estática |
| `state DOWN` | Interfaz deshabilitada | `ip link set <if> up` |
| `NO-CARRIER` | Sin enlace físico | Verificar cable/VM conectada |
| `169.254.x.x` (`scope link`) | DHCP falló | Revisar la red virtual |
| `dynamic` | Viene de DHCP | Normal en hosts y VMs |
| `valid_lft` bajo | Lease DHCP expira pronto | Se renueva automáticamente |

---

## 10. IPv4 en el contexto de QEMU/KVM

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

### IPs frecuentes

| IP | Dónde/qué es |
|----|-------------|
| 192.168.1.x | Tu máquina en la LAN doméstica |
| 192.168.122.1 | Host visto desde las VMs (gateway) |
| 192.168.122.100+ | VMs con DHCP de libvirt |
| 127.0.0.1 | Loopback (tanto en host como en cada VM) |
| 10.0.2.15 | IP por defecto en QEMU user-mode networking (sin libvirt) |

### Ver la IP de una VM sin entrar en ella

```bash
# Leases DHCP de la red default
virsh net-dhcp-leases default
# Expiry               MAC               Protocol  IP address       Hostname
# 2026-03-25 10:30:00  52:54:00:ab:cd:ef ipv4      192.168.122.100  fedora-vm

# Scan de la red (requiere nmap)
nmap -sn 192.168.122.0/24
```

---

## 11. Errores comunes

### Error 1: Confundir la IP del host con la IP del bridge

```bash
ip -brief addr
# enp0s3    UP    192.168.1.50/24       ← IP del host en la LAN
# virbr0    UP    192.168.122.1/24      ← IP del host en la red de VMs

# Desde una VM, el host se ve como 192.168.122.1, NO como 192.168.1.50
# 192.168.122.1 es la puerta de enlace directa de las VMs
```

El host tiene múltiples IPs — una por cada interfaz. El bridge virbr0 es una interfaz más del host con su propia IP en una red diferente.

### Error 2: Ver 169.254.x.x y pensar que funciona

Una IP 169.254.x.x (link-local) significa que **DHCP no respondió**. La VM no tendrá acceso a internet ni al host. Verificar: `virsh net-list --all`, `virsh domiflist <vm>`, `systemctl status libvirtd`.

### Error 3: Usar ifconfig en lugar de ip addr

`ifconfig` es del paquete `net-tools`, deprecated desde hace más de una década. No muestra todas las IPs secundarias y no existe en instalaciones mínimas modernas. Usar siempre `ip addr` o `ip -brief addr`.

### Error 4: Creer que ip addr "configura" la red

`ip addr` **solo muestra** información. Para asignar o modificar:

```bash
# IP temporal (se pierde al reiniciar):
sudo ip addr add 192.168.1.200/24 dev enp0s3

# Eliminar IP:
sudo ip addr del 192.168.1.200/24 dev enp0s3

# Para configuración permanente: NetworkManager o /etc/network/interfaces
```

### Error 5: Esperar que la VM tenga IP inmediatamente

Después de `virsh start <vm>`, la VM necesita: arrancar kernel → cargar drivers → DHCP Discover → recibir lease. Esto puede tardar 5-30 segundos. `virsh net-dhcp-leases default` no mostrará la VM si se consulta demasiado pronto.

---

## Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                    DIRECCIONES IPv4 CHEATSHEET                      ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  ESTRUCTURA                                                        ║
║  4 octetos × 8 bits = 32 bits total                                ║
║  Cada octeto: 0–255                                                ║
║  Espacio total: 2^32 = ~4.3 mil millones                           ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  TIPOS DE COMUNICACIÓN                                             ║
║  Unicast      1 → 1        (SSH, HTTP)                             ║
║  Broadcast    1 → todos    (DHCP discover, ARP)                    ║
║  Multicast    1 → grupo    (OSPF, mDNS)                            ║
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
║  COMANDOS                                                          ║
║  ip addr                     Todas las interfaces e IPs            ║
║  ip -4 addr                  Solo IPv4                              ║
║  ip -brief addr              Vista compacta                        ║
║  ip addr show <iface>        Una interfaz                           ║
║  ip -c addr                  Con colores                            ║
║  hostname -I                 Solo IPs (para scripts)                ║
║  virsh net-dhcp-leases X     Leases DHCP de libvirt                ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  DIAGNÓSTICO RÁPIDO                                                ║
║  Sin inet → no tiene IP (revisar DHCP/config)                      ║
║  169.254.x.x → DHCP falló                                          ║
║  NO-CARRIER → sin enlace (cable/VM desconectada)                   ║
║  state DOWN → interfaz deshabilitada                                ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1 — Inspección de interfaces con ip -brief addr

```bash
ip -brief addr
```

Preguntas:
1. ¿Cuántas interfaces de red tiene tu máquina (sin contar `lo`)?
2. ¿Cuáles están en estado UP y cuáles en DOWN?
3. ¿Alguna tiene más de una IP asignada?

<details><summary>Predicción</summary>

`ip -brief addr` muestra una línea por interfaz: nombre, estado, IPs. Verás al menos `lo` (UNKNOWN, 127.0.0.1/8) y una interfaz Ethernet (UP, con IP/máscara). Si tienes libvirt, aparecerá `virbr0` (UP o DOWN según si hay VMs). Si usas Docker, verás `docker0`. Las interfaces `vethXXXX` aparecen si hay contenedores corriendo. El estado UNKNOWN de `lo` es normal — loopback no tiene concepto de "enlace físico", así que nunca es UP ni DOWN.

</details>

### Ejercicio 2 — Desglose de ip addr completo

```bash
ip addr show <tu-interfaz-ethernet>
```

Preguntas:
1. ¿Tu IP fue asignada por DHCP o es estática? (busca `dynamic`).
2. ¿Cuál es la dirección de broadcast de tu subred?
3. ¿Cuál es la MAC address? ¿Reconoces el prefijo OUI?
4. ¿Qué scope tiene la IP (`global`, `link`, `host`)?
5. ¿Cuánto tiempo queda de lease DHCP? (`valid_lft`).

<details><summary>Predicción</summary>

- Si ves `dynamic` en la línea `inet`, la IP viene de DHCP. Si no, es estática. La mayoría de equipos domésticos usan DHCP.
- La dirección de broadcast aparece después de `brd` (e.g. `192.168.1.255` para una red /24).
- MAC: 6 pares hexadecimales. Los 3 primeros identifican el fabricante (OUI). `08:00:27` = VirtualBox, `52:54:00` = QEMU/KVM, `00:0c:29` = VMware.
- `scope global` = ruteable normalmente. `scope link` = solo enlace local (169.254.x.x). `scope host` = solo loopback.
- `valid_lft` muestra segundos restantes del lease. Típicamente 86400 (24h) o 3600 (1h). `forever` = estática.

</details>

### Ejercicio 3 — Conversión decimal a binario

Convierte **sin herramientas** (luego verifica):

```
a) 10.0.0.1        →  ¿?
b) 172.16.0.1      →  ¿?
c) 192.168.122.1   →  ¿?
```

Verificación:
```bash
python3 -c "print('.'.join(f'{int(o):08b}' for o in '10.0.0.1'.split('.')))"
python3 -c "print('.'.join(f'{int(o):08b}' for o in '172.16.0.1'.split('.')))"
python3 -c "print('.'.join(f'{int(o):08b}' for o in '192.168.122.1'.split('.')))"
```

<details><summary>Predicción</summary>

Usando la tabla de potencias (128, 64, 32, 16, 8, 4, 2, 1):

a) `10.0.0.1`:
- 10 = 8+2 = `00001010`
- 0 = `00000000`
- 0 = `00000000`
- 1 = `00000001`
- Resultado: `00001010.00000000.00000000.00000001`

b) `172.16.0.1`:
- 172 = 128+32+8+4 = `10101100`
- 16 = `00010000`
- 0 = `00000000`
- 1 = `00000001`
- Resultado: `10101100.00010000.00000000.00000001`

c) `192.168.122.1`:
- 192 = 128+64 = `11000000`
- 168 = 128+32+8 = `10101000`
- 122 = 64+32+16+8+2 = `01111010`
- 1 = `00000001`
- Resultado: `11000000.10101000.01111010.00000001`

</details>

### Ejercicio 4 — Conversión binario a decimal

Convierte **sin herramientas**:

```
d) 11111111.00000000.00000000.00000000  →  ¿?
e) 10101100.00010000.11111110.00000001  →  ¿?
f) 11000000.10101000.01111010.00000001  →  ¿?
```

Verificación:
```bash
echo "ibase=2; 11111111" | bc
echo "ibase=2; 10101100" | bc
```

Pregunta reflexiva: la dirección (d) es 255.0.0.0. ¿Es una dirección IP válida para asignar a un host? ¿Qué función cumple ese patrón de "todos unos"?

<details><summary>Predicción</summary>

d) Sumando posiciones con 1:
- `11111111` = 128+64+32+16+8+4+2+1 = **255**
- Los demás son 0
- Resultado: **255.0.0.0**

e) `10101100` = 128+32+8+4 = **172**; `00010000` = **16**; `11111110` = 128+64+32+16+8+4+2 = **254**; `00000001` = **1**
- Resultado: **172.16.254.1**

f) `11000000` = **192**; `10101000` = **168**; `01111010` = 64+32+16+8+2 = **122**; `00000001` = **1**
- Resultado: **192.168.122.1** (el gateway de libvirt)

**Pregunta reflexiva:** 255.0.0.0 NO se asigna a un host. Es una **máscara de red** — el patrón de "todos unos" en el primer octeto indica que esos 8 bits son la parte de red. Equivale a /8 en notación CIDR. Las máscaras de red se cubren en el siguiente tópico (T02).

</details>

### Ejercicio 5 — Identificar tipos de comunicación

Para cada escenario, indica si es unicast, broadcast o multicast:

1. `ssh user@192.168.122.100`
2. `ping 192.168.1.255` (en una red /24)
3. Un router OSPF envía actualizaciones a 224.0.0.5
4. Una VM nueva envía DHCP Discover
5. `curl http://192.168.122.1:8080`

<details><summary>Predicción</summary>

1. **Unicast** — SSH es comunicación directa entre tu máquina y la VM (1 a 1).
2. **Broadcast** — 192.168.1.255 es la dirección de broadcast de la red /24. El paquete llega a todos los hosts de esa subred.
3. **Multicast** — 224.0.0.5 es la dirección multicast de OSPF (AllSPFRouters). Solo los routers suscritos a ese grupo reciben el paquete.
4. **Broadcast** — DHCP Discover se envía a 255.255.255.255 (broadcast limitado) porque el cliente aún no tiene IP ni sabe la dirección del servidor DHCP.
5. **Unicast** — curl se conecta directamente al gateway (1 a 1).

</details>

### Ejercicio 6 — Diagnosticar direcciones especiales

¿Qué significa cada salida y qué harías?

```bash
# Caso A:
ip addr show eth0
# inet 169.254.45.67/16 scope link

# Caso B:
ss -tlnp | grep 3306
# LISTEN  0  128  127.0.0.1:3306  0.0.0.0:*  users:(("mysqld",pid=2345))

# Caso C:
ss -tlnp | grep 80
# LISTEN  0  128  0.0.0.0:80  0.0.0.0:*  users:(("nginx",pid=1234))
```

<details><summary>Predicción</summary>

**Caso A:** IP 169.254.x.x (link-local/APIPA) → **DHCP falló**. La VM se auto-asignó una IP temporal. No tendrá acceso a internet ni probablemente al host. Acción: verificar que la red virtual esté activa (`virsh net-list --all`), que la VM esté conectada a la red correcta (`virsh domiflist <vm>`), y que libvirtd esté corriendo.

**Caso B:** MySQL escucha en `127.0.0.1:3306` → solo acepta conexiones **locales** (desde la misma máquina). Si otra VM o el host intenta conectarse a MySQL desde fuera, será rechazado. Para permitir conexiones remotas, MySQL debe escuchar en `0.0.0.0:3306` o en la IP específica de la interfaz.

**Caso C:** nginx escucha en `0.0.0.0:80` → acepta conexiones desde **cualquier interfaz**. Es accesible tanto desde loopback (127.0.0.1) como desde la red (e.g. 192.168.122.x). Cualquier VM o host que pueda llegar a esta máquina puede acceder al puerto 80.

</details>

### Ejercicio 7 — Interpretar flags de ip addr

Analiza la salida e identifica el estado de cada interfaz:

```
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 state UNKNOWN
    inet 127.0.0.1/8 scope host lo

2: enp0s3: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 state UP
    link/ether 08:00:27:ab:cd:ef brd ff:ff:ff:ff:ff:ff
    inet 192.168.1.50/24 brd 192.168.1.255 scope global dynamic enp0s3
       valid_lft 43200sec preferred_lft 43200sec

5: virbr0: <NO-CARRIER,BROADCAST,MULTICAST,UP> mtu 1500 state DOWN
    link/ether 52:54:00:12:34:56 brd ff:ff:ff:ff:ff:ff
    inet 192.168.122.1/24 brd 192.168.122.255 scope global virbr0
       valid_lft forever preferred_lft forever
```

Preguntas:
1. ¿Por qué `lo` tiene state UNKNOWN?
2. ¿`enp0s3` tiene cable conectado? ¿Cómo lo sabes?
3. ¿Hay VMs corriendo en virbr0? ¿Cómo lo deduces?
4. ¿La IP de enp0s3 es estática o DHCP? ¿Cuánto falta para que expire?
5. ¿La IP de virbr0 es estática o DHCP?

<details><summary>Predicción</summary>

1. `lo` tiene state UNKNOWN porque loopback no tiene concepto de enlace físico. No puede estar "UP" (cable conectado) ni "DOWN" (desconectado). UNKNOWN es su estado normal y permanente.

2. Sí, `enp0s3` tiene cable conectado (o conexión activa). Lo sabes por el flag `LOWER_UP` que indica enlace físico activo, y `state UP` lo confirma. Si no hubiera cable, verías `NO-CARRIER` y `state DOWN`.

3. No hay VMs corriendo. `virbr0` tiene `NO-CARRIER` (nadie conectado al bridge) y `state DOWN`. Cuando una VM arranca y se conecta al bridge vía vnetN, el estado cambiaría a `LOWER_UP` y `state UP`.

4. DHCP — tiene la palabra `dynamic` en la línea `inet`. Faltan 43200 segundos (12 horas) para que expire el lease.

5. Estática — `valid_lft forever`. La IP 192.168.122.1 la asigna libvirt al crear el bridge, no viene de DHCP.

</details>

### Ejercicio 8 — Red default de libvirt

```bash
# Si tienes libvirt instalado:
virsh net-dumpxml default 2>/dev/null || echo "libvirt no disponible"
```

Si no tienes libvirt, analiza esta salida XML:

```xml
<network>
  <name>default</name>
  <bridge name='virbr0' stp='on' delay='0'/>
  <ip address='192.168.122.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='192.168.122.2' end='192.168.122.254'/>
    </dhcp>
  </ip>
</network>
```

Preguntas:
1. ¿Cuántas IPs puede asignar DHCP en esta red?
2. ¿Qué IP tiene el gateway (el host)?
3. ¿Cuál es la dirección de broadcast?
4. ¿Cuál es la dirección de red?
5. Si una VM obtiene 192.168.122.100, ¿puede hacer ping a 192.168.122.1?

<details><summary>Predicción</summary>

1. Rango DHCP: 192.168.122.2 a 192.168.122.254 = **253 IPs** disponibles. (192.168.122.1 es el gateway, .0 es la red, .255 es broadcast).
2. Gateway: **192.168.122.1** — es la IP del bridge virbr0 en el host.
3. Broadcast: **192.168.122.255** — última dirección de una red /24 (netmask 255.255.255.0).
4. Dirección de red: **192.168.122.0** — primera dirección, identifica la subred.
5. Sí, puede hacer ping a 192.168.122.1. Es su gateway directo y está en la misma subred /24. La VM tiene conectividad directa al bridge del host.

</details>

### Ejercicio 9 — Diagnóstico de VM sin red

Imagina que arrancaste una VM con `virsh start fedora-vm` y quieres conectarte por SSH, pero `virsh net-dhcp-leases default` no muestra nada.

Describe los pasos de diagnóstico en orden:

1. ¿Qué verificas primero?
2. ¿Y si `virsh net-list --all` muestra la red como "inactive"?
3. ¿Y si la VM tiene IP 169.254.x.x?
4. ¿Y si la VM tiene 192.168.122.105, cómo te conectas?
5. Dentro de la VM con `ip addr`, ¿esperarías ver `virbr0`? ¿Por qué?

<details><summary>Predicción</summary>

1. Primero: esperar 15-30 segundos (la VM necesita tiempo para arrancar y completar DHCP). Luego `virsh net-dhcp-leases default` de nuevo. Si sigue vacío, verificar que la VM está corriendo: `virsh list --all`.

2. Si la red está "inactive": `virsh net-start default` para activarla. Luego reiniciar la VM o esperar a que el DHCP client reintente. Sin red activa, no hay servidor DHCP y la VM no puede obtener IP.

3. 169.254.x.x = DHCP falló. Verificar: ¿la red está activa?, ¿la VM está conectada a la red correcta? (`virsh domiflist fedora-vm`), ¿libvirtd está corriendo? Posible solución: `virsh net-start default` y luego `dhclient eth0` dentro de la VM.

4. Con 192.168.122.105: `ssh user@192.168.122.105` desde el host. El host tiene acceso directo a la red 192.168.122.0/24 a través de virbr0.

5. **No** verías `virbr0` dentro de la VM. El bridge vive en el **host**, no en la VM. La VM solo ve su propia interfaz (e.g. `eth0` o `enp1s0`) con la IP asignada. virbr0 es el switch virtual del host al que se conecta la VM vía TAP device (vnetN).

</details>

### Ejercicio 10 — Escenario completo: ¿quién puede hablar con quién?

Dado este escenario:

```
Host: enp0s3 = 192.168.1.50/24, virbr0 = 192.168.122.1/24
VM1:  eth0   = 192.168.122.100/24
VM2:  eth0   = 192.168.122.101/24
Vecino en LAN: 192.168.1.80/24
```

¿Quién puede hacer ping a quién?

| Desde → Hacia | Host (1.50) | Host (122.1) | VM1 (122.100) | VM2 (122.101) | Vecino (1.80) |
|---------------|:-----------:|:------------:|:--------------:|:--------------:|:-------------:|
| Host          |      —      |      ?       |       ?        |       ?        |       ?       |
| VM1           |      ?      |      ?       |       —        |       ?        |       ?       |
| VM2           |      ?      |      ?       |       ?        |       —        |       ?       |
| Vecino        |      ?      |      ?       |       ?        |       ?        |       —       |

<details><summary>Predicción</summary>

| Desde → Hacia | Host (1.50) | Host (122.1) | VM1 (122.100) | VM2 (122.101) | Vecino (1.80) |
|---------------|:-----------:|:------------:|:--------------:|:--------------:|:-------------:|
| Host          |      —      |      Sí      |       Sí       |       Sí       |      Sí       |
| VM1           |     Sí*     |      Sí      |       —        |       Sí       |     Sí*       |
| VM2           |     Sí*     |      Sí      |       Sí       |       —        |     Sí*       |
| Vecino        |      Sí     |      No      |       No       |       No       |       —       |

Explicación:

- **Host → todo**: el host tiene interfaces en ambas redes (1.0/24 y 122.0/24), puede alcanzar todo.
- **VM → Host (1.50)**: Sí*, vía NAT. La VM envía al gateway (122.1), que hace NAT y reenvía a la red 1.0/24.
- **VM → Host (122.1)**: Sí, misma subred directamente.
- **VM1 ↔ VM2**: Sí, ambas en la misma subred 122.0/24 a través del bridge virbr0.
- **VM → Vecino (1.80)**: Sí*, vía NAT. Sale por el gateway 122.1 y el host hace NAT hacia la LAN.
- **Vecino → Host (1.50)**: Sí, misma subred.
- **Vecino → VMs o Host (122.1)**: **No**. El vecino no tiene ruta a la red 192.168.122.0/24. Esa red es privada del host, detrás de NAT. El vecino solo conoce la red 192.168.1.0/24. Para acceder, necesitaría port forwarding configurado en el host.

Clave: la red de libvirt es NAT — las VMs pueden salir, pero desde fuera no se puede entrar sin port forwarding.

</details>
