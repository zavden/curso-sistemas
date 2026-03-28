# T02 — Bonding y Teaming: Alta Disponibilidad de Red y Modos de Bonding

## Índice

1. [¿Por qué agregar enlaces?](#1-por-qué-agregar-enlaces)
2. [Bonding vs Teaming](#2-bonding-vs-teaming)
3. [Modos de bonding](#3-modos-de-bonding)
4. [Monitoreo de enlaces: MII y ARP](#4-monitoreo-de-enlaces-mii-y-arp)
5. [Configurar bonding con ip/sysfs](#5-configurar-bonding-con-ipsysfs)
6. [Configurar bonding con NetworkManager](#6-configurar-bonding-con-networkmanager)
7. [Configurar bonding con systemd-networkd](#7-configurar-bonding-con-systemd-networkd)
8. [Configurar bonding con ifupdown](#8-configurar-bonding-con-ifupdown)
9. [LACP (802.3ad): configuración del switch](#9-lacp-8023ad-configuración-del-switch)
10. [Diagnóstico y monitoreo](#10-diagnóstico-y-monitoreo)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. ¿Por qué agregar enlaces?

La agregación de enlaces combina múltiples interfaces de red físicas en una sola interfaz lógica. Esto proporciona dos beneficios fundamentales:

```
Sin bonding:                        Con bonding:

┌──────────┐                        ┌──────────┐
│ Servidor │                        │ Servidor │
│          │                        │          │
│   eth0 ──┼──── 1 Gbps ──►Switch  │  bond0 ──┼──── 2 Gbps ──►Switch
│   eth1   │  (sin usar)           │  ├ eth0  │  (ambas activas)
│          │                        │  └ eth1  │
└──────────┘                        └──────────┘
Si eth0 falla → sin red            Si eth0 falla → eth1 sigue

Beneficio 1: REDUNDANCIA (alta disponibilidad)
Beneficio 2: MAYOR ANCHO DE BANDA (en algunos modos)
```

Escenarios donde el bonding es esencial:

| Escenario | Prioridad | Modo típico |
|-----------|-----------|-------------|
| Servidor de base de datos | Redundancia | active-backup (modo 1) |
| Servidor de archivos / NAS | Ancho de banda + redundancia | 802.3ad (modo 4) |
| Hipervisor (KVM, VMware) | Ambos | 802.3ad o balance-alb |
| Servidor web frontend | Redundancia | active-backup |
| Storage iSCSI | Ancho de banda | 802.3ad con multipath |

---

## 2. Bonding vs Teaming

Linux ofrece dos mecanismos para agregar enlaces: **bonding** (kernel module) y **teaming** (userspace daemon). Ambos logran resultados similares pero con arquitecturas diferentes:

```
Bonding:                            Teaming:
┌─────────────────────┐             ┌─────────────────────┐
│  Kernel module       │             │  teamd (userspace)  │
│  (bonding.ko)        │             │  + kernel module    │
│                      │             │  (team.ko)          │
│  Configuración:      │             │                     │
│  sysfs / modprobe    │             │  Configuración:     │
│                      │             │  JSON               │
│  Todo en kernel →    │             │                     │
│  máximo rendimiento  │             │  Más flexible,      │
│                      │             │  extensible          │
│  Ampliamente usado   │             │  Menos extendido     │
│  Documentación vasta │             │                     │
└─────────────────────┘             └─────────────────────┘
```

| Criterio | Bonding | Teaming |
|----------|---------|---------|
| Implementación | Módulo kernel | Daemon userspace + módulo kernel |
| Rendimiento | Ligeramente mejor (todo en kernel) | Muy similar |
| Configuración | sysfs, archivos de conf | JSON |
| Modos | 7 modos fijos | "runners" extensibles |
| Monitoreo | MII, ARP | MII, ARP, más extensible |
| Madurez | Desde kernel 2.4 (~2001) | Desde RHEL 7 (~2014) |
| Soporte en RHEL 9 | Sí | Sí (pero bonding es preferido) |
| Comunidad | Estándar de facto | Menor adopción |

> **Recomendación práctica**: usa **bonding**. Es el estándar de facto, tiene más documentación, es soportado por todas las distribuciones y herramientas, y la diferencia de funcionalidad con teaming es mínima para la mayoría de los casos. Este tema se centra en bonding.

---

## 3. Modos de bonding

El módulo bonding soporta 7 modos. Cada uno determina cómo se distribuye el tráfico y cómo se maneja la redundancia:

### Modo 0: balance-rr (Round Robin)

```
Paquete 1 → eth0 → Switch
Paquete 2 → eth1 → Switch
Paquete 3 → eth0 → Switch
Paquete 4 → eth1 → Switch
...

✓ Distribución uniforme de tráfico
✓ Tolerancia a fallos
✗ Requiere configuración en el switch (EtherChannel/port-channel)
✗ Puede causar paquetes desordenados (problema para TCP)
```

### Modo 1: active-backup

```
                 ┌── eth0 (ACTIVE) ──► Switch
bond0 ───────────┤
                 └── eth1 (BACKUP) ──  (standby, no transmite)

Si eth0 falla:
                 ┌── eth0 (FAILED)
bond0 ───────────┤
                 └── eth1 (ACTIVE) ──► Switch  (toma el control)

✓ No requiere configuración en el switch
✓ Tolerancia a fallos simple y fiable
✓ Compatible con cualquier switch
✗ Solo usa un enlace a la vez (no suma ancho de banda)
```

Este es el modo más simple y seguro. Ideal cuando la prioridad es redundancia sin complicar el switch.

### Modo 2: balance-xor

```
Hash del paquete (MAC origen XOR MAC destino) → elige interfaz

Tráfico a MAC-A → siempre eth0
Tráfico a MAC-B → siempre eth1

✓ Distribución por flujo (no desordena paquetes)
✓ Tolerancia a fallos
✗ Requiere configuración en el switch
✗ Distribución puede no ser uniforme si hay pocos destinos
```

### Modo 3: broadcast

```
Cada paquete se envía por TODAS las interfaces:

bond0 → eth0 → Switch (copia 1)
      → eth1 → Switch (copia 2)

✓ Máxima tolerancia a fallos
✗ No suma ancho de banda (mismo paquete duplicado)
✗ Muy específico: solo útil para escenarios de alta disponibilidad extrema
```

### Modo 4: 802.3ad (LACP)

```
LACP (Link Aggregation Control Protocol):

┌──────────┐         LACP PDU          ┌──────────┐
│ Servidor │◄═══════════════════════►  │  Switch  │
│          │   "Negociemos agregación"  │          │
│  bond0   │                           │ port-ch  │
│  ├ eth0 ═╪═══════════════════════════╪═ Gi0/1  │
│  └ eth1 ═╪═══════════════════════════╪═ Gi0/2  │
└──────────┘                           └──────────┘

El protocolo LACP negocia:
  - Qué puertos se agregan
  - Detección de fallos (LACPDU cada 1s o 30s)
  - Distribución de tráfico (hash configurable)

✓ Estándar IEEE → compatible entre fabricantes
✓ Suma ancho de banda real
✓ Detección rápida de fallos (fast LACP = 1s)
✓ Tolerancia a fallos negociada
✗ Requiere switch con soporte LACP
✗ Configuración en ambos lados (servidor + switch)
```

**Este es el modo más usado en data centers** por su combinación de rendimiento, redundancia y estandarización.

### Modo 5: balance-tlb (Transmit Load Balancing)

```
Transmisión: distribuida por carga entre todas las interfaces
Recepción: solo por una interfaz (la activa)

TX: bond0 → eth0 (host A, menos cargada)
         → eth1 (host B, menos cargada)
RX: todo llega por eth0 (slave activo)

✓ No requiere configuración en el switch
✓ Balanceo de salida sin soporte especial
✗ Recepción limitada a un enlace
✗ Requiere soporte de cambio de MAC en el driver
```

### Modo 6: balance-alb (Adaptive Load Balancing)

```
Igual que balance-tlb, pero TAMBIÉN balancea recepción:

TX: distribuido adaptivamente entre interfaces
RX: distribuido manipulando respuestas ARP
    (el bond envía diferentes MACs a diferentes peers)

✓ No requiere configuración en el switch
✓ Balancea tanto TX como RX
✓ El más inteligente para entornos sin LACP
✗ Manipulación ARP puede confundir a algunos switches/IDS
✗ Requiere soporte de cambio de MAC en el driver
```

### Tabla resumen

```
┌───────┬─────────────────┬──────────┬──────────┬──────────────┐
│ Modo  │ Nombre          │ TX dist. │ RX dist. │ Switch config│
├───────┼─────────────────┼──────────┼──────────┼──────────────┤
│   0   │ balance-rr      │ Round-r. │ Round-r. │ SÍ           │
│   1   │ active-backup   │ 1 sola   │ 1 sola   │ NO           │
│   2   │ balance-xor     │ Hash     │ Hash     │ SÍ           │
│   3   │ broadcast       │ Todas    │ 1 sola   │ SÍ           │
│   4   │ 802.3ad (LACP)  │ Hash     │ Hash     │ SÍ (LACP)    │
│   5   │ balance-tlb     │ Carga    │ 1 sola   │ NO           │
│   6   │ balance-alb     │ Carga    │ ARP bal. │ NO           │
└───────┴─────────────────┴──────────┴──────────┴──────────────┘

Recomendación rápida:
  ¿Switch soporta LACP?
  ├── SÍ → Modo 4 (802.3ad)
  └── NO → ¿Necesitas ancho de banda?
            ├── SÍ → Modo 6 (balance-alb)
            └── NO → Modo 1 (active-backup)
```

---

## 4. Monitoreo de enlaces: MII y ARP

El bonding necesita detectar cuándo un enlace falla para hacer failover. Hay dos métodos de monitoreo:

### MII monitor (Media Independent Interface)

Monitorea el estado del enlace a nivel de driver (carrier up/down):

```
MII Monitor:

bond0 verifica cada N ms:
  eth0: ¿carrier? → SÍ (link UP)
  eth1: ¿carrier? → NO (link DOWN) → FAILOVER

Detecta:
  ✓ Cable desconectado
  ✓ Puerto del switch caído
  ✓ Fallo de hardware en la NIC

No detecta:
  ✗ Switch vivo pero sin ruta (upstream muerto)
  ✗ Problemas de capa 3 (IP mal configurada)
  ✗ Firewall bloqueando tráfico
```

```bash
# Configurar MII monitor (intervalo en ms)
# Valor típico: 100ms
# /sys/class/net/bond0/bonding/miimon
echo 100 > /sys/class/net/bond0/bonding/miimon
```

### ARP monitor

Envía ARP requests periódicos a un target y verifica que responda:

```
ARP Monitor:

bond0 envía ARP request a 192.168.1.1 cada N ms:
  eth0: → ARP request → ¿respuesta? → SÍ (enlace OK)
  eth1: → ARP request → ¿respuesta? → NO → FAILOVER

Detecta TODO lo que detecta MII, ADEMÁS de:
  ✓ Switch vivo pero sin ruta upstream
  ✓ Problemas de red más allá del enlace directo
  ✓ Configuración IP incorrecta

Limitaciones:
  ✗ Genera tráfico ARP adicional
  ✗ Depende de que el target responda ARP
  ✗ No funciona en modo 802.3ad (usar MII ahí)
```

```bash
# Configurar ARP monitor
# Intervalo en ms
echo 200 > /sys/class/net/bond0/bonding/arp_interval

# Target(s) para ARP — típicamente el gateway
echo 192.168.1.1 > /sys/class/net/bond0/bonding/arp_ip_target
# Múltiples targets:
echo +192.168.1.2 > /sys/class/net/bond0/bonding/arp_ip_target
```

**MII vs ARP**: son mutuamente excluyentes. Usa MII como default (más simple, menor overhead). Usa ARP si necesitas detectar problemas más allá del enlace físico.

### Parámetros de failover

```bash
# Tiempo antes de activar un enlace que volvió (evita flapping)
# updelay: esperar N ms tras link-up antes de usar el enlace
echo 200 > /sys/class/net/bond0/bonding/updelay

# downdelay: esperar N ms tras link-down antes de failover
echo 0 > /sys/class/net/bond0/bonding/downdelay
# (generalmente 0: failover inmediato al detectar fallo)

# updelay y downdelay deben ser múltiplos de miimon
```

---

## 5. Configurar bonding con ip/sysfs

Configuración manual con comandos `ip` y sysfs. Útil para entender qué pasa internamente y para testing:

```bash
# 1. Cargar el módulo
sudo modprobe bonding

# 2. Crear la interfaz bond
sudo ip link add bond0 type bond

# 3. Configurar el modo (ANTES de agregar slaves)
echo 802.3ad > /sys/class/net/bond0/bonding/mode
# O para active-backup:
# echo active-backup > /sys/class/net/bond0/bonding/mode

# 4. Configurar monitoreo
echo 100 > /sys/class/net/bond0/bonding/miimon

# 5. Configurar parámetros específicos del modo
# Para 802.3ad:
echo fast > /sys/class/net/bond0/bonding/lacp_rate
echo layer3+4 > /sys/class/net/bond0/bonding/xmit_hash_policy

# 6. Bajar las interfaces miembros
sudo ip link set eth0 down
sudo ip link set eth1 down

# 7. Agregar slaves al bond
sudo ip link set eth0 master bond0
sudo ip link set eth1 master bond0

# 8. Subir todo
sudo ip link set bond0 up
sudo ip link set eth0 up
sudo ip link set eth1 up

# 9. Asignar IP al bond (no a las interfaces individuales)
sudo ip addr add 192.168.1.50/24 dev bond0
sudo ip route add default via 192.168.1.1 dev bond0
```

> **Recuerda**: esta configuración es temporal. Se pierde al reiniciar. Usa un gestor de red para persistirla.

---

## 6. Configurar bonding con NetworkManager

### Modo active-backup (más simple)

```bash
# Crear el bond
nmcli connection add type bond con-name bond0 ifname bond0 \
    bond.options "mode=active-backup,miimon=100"

# Agregar slaves
nmcli connection add type ethernet con-name bond0-eth0 ifname eth0 \
    master bond0
nmcli connection add type ethernet con-name bond0-eth1 ifname eth1 \
    master bond0

# Configurar IP (DHCP)
nmcli connection modify bond0 ipv4.method auto

# O IP estática
nmcli connection modify bond0 \
    ipv4.method manual \
    ipv4.addresses "192.168.1.50/24" \
    ipv4.gateway "192.168.1.1" \
    ipv4.dns "192.168.1.1"

# Activar
nmcli connection up bond0
```

### Modo 802.3ad (LACP)

```bash
nmcli connection add type bond con-name bond0 ifname bond0 \
    bond.options "mode=802.3ad,miimon=100,lacp_rate=fast,xmit_hash_policy=layer3+4"

nmcli connection add type ethernet con-name bond0-eth0 ifname eth0 \
    master bond0
nmcli connection add type ethernet con-name bond0-eth1 ifname eth1 \
    master bond0

nmcli connection modify bond0 \
    ipv4.method manual \
    ipv4.addresses "192.168.1.50/24" \
    ipv4.gateway "192.168.1.1"

nmcli connection up bond0
```

### Verificar

```bash
# Estado del bond
nmcli device show bond0

# Ver opciones del bond
nmcli -f bond connection show bond0
```

### Modificar opciones después de crear

```bash
# Cambiar modo (requiere reactivar)
nmcli connection modify bond0 bond.options "mode=balance-alb,miimon=100"
nmcli connection down bond0 && nmcli connection up bond0

# Agregar un slave más
nmcli connection add type ethernet con-name bond0-eth2 ifname eth2 \
    master bond0
```

---

## 7. Configurar bonding con systemd-networkd

### active-backup

```ini
# /etc/systemd/network/20-bond0.netdev
[NetDev]
Name=bond0
Kind=bond

[Bond]
Mode=active-backup
MIIMonitorSec=100ms
PrimaryReselectPolicy=always
```

```ini
# /etc/systemd/network/25-bond0-members.network
[Match]
Name=eth0 eth1

[Network]
Bond=bond0
```

```ini
# /etc/systemd/network/30-bond0.network
[Match]
Name=bond0

[Network]
Address=192.168.1.50/24
Gateway=192.168.1.1
DNS=192.168.1.1
```

### 802.3ad (LACP)

```ini
# /etc/systemd/network/20-bond0.netdev
[NetDev]
Name=bond0
Kind=bond

[Bond]
Mode=802.3ad
MIIMonitorSec=100ms
LACPTransmitRate=fast
TransmitHashPolicy=layer3+4
MinLinks=1
```

```ini
# /etc/systemd/network/25-bond0-members.network
[Match]
Name=eth0 eth1

[Network]
Bond=bond0
```

```ini
# /etc/systemd/network/30-bond0.network
[Match]
Name=bond0

[Network]
DHCP=yes
```

### Parámetros de [Bond] en systemd-networkd

| Parámetro | Equivalente en bonding | Valores |
|-----------|----------------------|---------|
| `Mode=` | `mode` | balance-rr, active-backup, balance-xor, broadcast, 802.3ad, balance-tlb, balance-alb |
| `MIIMonitorSec=` | `miimon` | Tiempo (100ms) |
| `UpDelaySec=` | `updelay` | Tiempo (200ms) |
| `DownDelaySec=` | `downdelay` | Tiempo (0ms) |
| `LACPTransmitRate=` | `lacp_rate` | slow, fast |
| `TransmitHashPolicy=` | `xmit_hash_policy` | layer2, layer2+3, layer3+4, encap2+3, encap3+4 |
| `MinLinks=` | `min_links` | Número mínimo de enlaces activos |
| `ARPIntervalSec=` | `arp_interval` | Tiempo |
| `ARPIPTargets=` | `arp_ip_target` | IPs separadas por espacio |
| `PrimaryReselectPolicy=` | `primary_reselect` | always, better, failure |

---

## 8. Configurar bonding con ifupdown

```
# /etc/network/interfaces

auto bond0
iface bond0 inet static
    address 192.168.1.50/24
    gateway 192.168.1.1
    dns-nameservers 192.168.1.1

    bond-slaves eth0 eth1
    bond-mode active-backup
    bond-miimon 100
    bond-primary eth0

auto eth0
iface eth0 inet manual
    bond-master bond0

auto eth1
iface eth1 inet manual
    bond-master bond0
```

Para LACP:

```
auto bond0
iface bond0 inet static
    address 192.168.1.50/24
    gateway 192.168.1.1

    bond-slaves eth0 eth1
    bond-mode 802.3ad
    bond-miimon 100
    bond-lacp-rate fast
    bond-xmit-hash-policy layer3+4

auto eth0
iface eth0 inet manual
    bond-master bond0

auto eth1
iface eth1 inet manual
    bond-master bond0
```

Requiere el paquete `ifenslave`:

```bash
sudo apt install ifenslave
```

---

## 9. LACP (802.3ad): configuración del switch

El modo 802.3ad (LACP) requiere configuración **en ambos lados**: servidor y switch. Si el switch no tiene LACP habilitado, el bond no negocia y los enlaces no se agregan.

### Concepto

```
Sin LACP en el switch:
┌──────────┐                 ┌──────────┐
│ Servidor │                 │  Switch  │
│  bond0   │                 │          │
│  ├ eth0 ═╪═══════════════  ╪═ Gi0/1  │  ← Puerto independiente
│  └ eth1 ═╪═══════════════  ╪═ Gi0/2  │  ← Puerto independiente
└──────────┘                 └──────────┘
El switch ve 2 puertos normales. El bond envía LACPDU
pero el switch no responde. Resultado: NO funciona.

Con LACP en el switch:
┌──────────┐    LACPDU       ┌──────────┐
│ Servidor │◄═══════════════►│  Switch  │
│  bond0   │   Negociación   │ Port-ch1 │
│  ├ eth0 ═╪═══════════════  ╪═ Gi0/1 ─┤
│  └ eth1 ═╪═══════════════  ╪═ Gi0/2 ─┤  ← Agrupados en port-channel
└──────────┘                 └──────────┘
```

### Ejemplo en switches comunes

```
# Cisco IOS:
interface range GigabitEthernet0/1-2
  channel-group 1 mode active
  ! "active" = LACP activo (envía LACPDU)
  ! "passive" = LACP pasivo (solo responde)

interface Port-channel1
  switchport mode trunk
  ! o: switchport mode access / switchport access vlan X

# Ejemplo con verificación:
show etherchannel summary
show lacp neighbor
```

```
# Juniper JunOS:
set interfaces ae0 aggregated-ether-options lacp active
set interfaces ge-0/0/0 ether-options 802.3ad ae0
set interfaces ge-0/0/1 ether-options 802.3ad ae0
```

### Hash policy (xmit_hash_policy)

La hash policy determina cómo se distribuyen los flujos entre los enlaces:

```
layer2:        Hash basado en MAC origen + destino
               → Todo el tráfico a un mismo MAC va por el mismo enlace
               → Malo si hay pocos destinos MAC (ej: un solo gateway)

layer3+4:      Hash basado en IP + puerto origen/destino
               → Distribución por flujo TCP/UDP
               → RECOMENDADO para la mayoría de escenarios

layer2+3:      Hash basado en MAC + IP
               → Compromiso entre layer2 y layer3+4

encap2+3:      Para tráfico encapsulado (VXLAN, GRE)
encap3+4:      Para tráfico encapsulado con puertos
```

```
Ejemplo de distribución con layer3+4:

Servidor bond0 (eth0 + eth1) → Gateway 192.168.1.1

Flujo 1: 192.168.1.50:45000 → 10.0.0.1:443   → hash → eth0
Flujo 2: 192.168.1.50:45001 → 10.0.0.2:80     → hash → eth1
Flujo 3: 192.168.1.50:45002 → 10.0.0.1:8080   → hash → eth0
Flujo 4: 192.168.1.50:45003 → 10.0.0.3:443    → hash → eth1

Un FLUJO individual nunca excede el ancho de banda de UN enlace.
El beneficio es tener MÚLTIPLES flujos distribuidos.
```

> **Concepto clave**: el bonding no acelera una conexión individual. Una descarga de un archivo desde un solo servidor seguirá limitada a la velocidad de un enlace. El bonding multiplica el ancho de banda **agregado** cuando hay múltiples flujos simultáneos.

---

## 10. Diagnóstico y monitoreo

### /proc/net/bonding

```bash
cat /proc/net/bonding/bond0
```

```
Ethernet Channel Bonding Driver: v5.15.0

Bonding Mode: IEEE 802.3ad Dynamic link aggregation
Transmit Hash Policy: layer3+4 (1)
MII Status: up
MII Polling Interval (ms): 100
Up Delay (ms): 200
Down Delay (ms): 0
Peer Notification Delay (ms): 0

802.3ad info
LACP active: on
LACP rate: fast
Min links: 0
Aggregator selection policy (ad_select): stable
System priority: 65535
System MAC address: aa:bb:cc:11:22:33
Active Aggregator Info:
        Aggregator ID: 1
        Number of ports: 2
        Actor Key: 9
        Partner Key: 1
        Partner Mac Address: 00:11:22:33:44:55

Slave Interface: eth0
MII Status: up
Speed: 1000 Mbps
Duplex: full
Link Failure Count: 0
Permanent HW addr: aa:bb:cc:11:22:33
Slave queue ID: 0
Aggregator ID: 1
Actor Churn State: none
Partner Churn State: none
Actor Churned Count: 0
Partner Churned Count: 0

Slave Interface: eth1
MII Status: up
Speed: 1000 Mbps
Duplex: full
Link Failure Count: 0
Permanent HW addr: dd:ee:ff:44:55:66
Slave queue ID: 0
Aggregator ID: 1
Actor Churn State: none
Partner Churn State: none
```

Información clave a verificar:

| Campo | Qué buscar |
|-------|------------|
| `Bonding Mode` | Que sea el modo esperado |
| `MII Status` | `up` en bond y cada slave |
| `Speed` | Que ambos slaves tengan la misma velocidad |
| `Link Failure Count` | 0 en operación normal; incrementa en cada fallo |
| `Number of ports` | Que coincida con el número de slaves |
| `Partner Mac Address` | Que sea la MAC del switch (solo en LACP) |
| `Aggregator ID` | Que todos los slaves tengan el mismo (misma agregación) |

### Sysfs

```bash
# Modo actual
cat /sys/class/net/bond0/bonding/mode
# 802.3ad 4

# Slaves actuales
cat /sys/class/net/bond0/bonding/slaves
# eth0 eth1

# Slave activo (en active-backup)
cat /sys/class/net/bond0/bonding/active_slave
# eth0

# Estado MII de cada slave
cat /sys/class/net/bond0/bonding/mii_status
# up

# LACP rate
cat /sys/class/net/bond0/bonding/lacp_rate
# fast 1

# Hash policy
cat /sys/class/net/bond0/bonding/xmit_hash_policy
# layer3+4 1

# Link failures
cat /sys/class/net/bond0/slave_eth0/bonding_slave/link_failure_count
# 0
```

### ip link show

```bash
ip link show bond0
```

```
5: bond0: <BROADCAST,MULTICAST,MASTER,UP,LOWER_UP> mtu 1500 qdisc noqueue state UP
    link/ether aa:bb:cc:11:22:33 brd ff:ff:ff:ff:ff:ff
```

```bash
ip link show eth0
```

```
2: eth0: <BROADCAST,MULTICAST,SLAVE,UP,LOWER_UP> mtu 1500 qdisc fq_codel master bond0 state UP
    link/ether aa:bb:cc:11:22:33 brd ff:ff:ff:ff:ff:ff
```

Observa los flags: `MASTER` en bond0, `SLAVE` en eth0, y `master bond0` indica la pertenencia.

### Estadísticas

```bash
# Estadísticas de tráfico por slave
ip -s link show eth0
ip -s link show eth1

# Comparar TX bytes entre slaves:
# En modo 802.3ad con buen balanceo, deberían ser similares
# En active-backup, solo el activo tiene tráfico

# Monitoreo continuo con watch
watch -n 1 cat /proc/net/bonding/bond0
```

### Logs

```bash
# Eventos de bonding en logs
journalctl -k | grep -i bond
# o
dmesg | grep -i bond

# Ejemplo de failover:
# bond0: link status definitely down for interface eth0, disabling it
# bond0: making interface eth1 the new active one
# bond0: link status definitely up for interface eth0, 1000 Mbps full duplex
# bond0: making interface eth0 the new active one
```

---

## 11. Errores comunes

### Error 1: No configurar LACP en el switch para modo 802.3ad

```
PROBLEMA:
# Servidor configurado con mode=802.3ad
# Switch con puertos normales (sin LACP)

cat /proc/net/bonding/bond0
# Number of ports: 0    ← Sin agregación
# Partner Mac Address: 00:00:00:00:00:00  ← Sin partner

# Los enlaces están UP pero no se agregan

SOLUCIÓN:
# Configurar LACP en los puertos del switch
# O cambiar el modo a uno que no requiera switch:
# - active-backup (modo 1)
# - balance-alb (modo 6)
```

### Error 2: Asignar IP a las interfaces individuales en vez del bond

```
INCORRECTO:
ip addr add 192.168.1.50/24 dev eth0    ← IP en el slave
ip addr add 192.168.1.51/24 dev eth1    ← IP en el slave
# eth0 y eth1 son parte de bond0, no deben tener IP propia

CORRECTO:
# Los slaves NO tienen IP
# Solo el bond tiene IP
ip addr add 192.168.1.50/24 dev bond0
```

### Error 3: Slaves con diferente velocidad/duplex

```
PROBLEMA:
cat /proc/net/bonding/bond0
# Slave Interface: eth0
# Speed: 1000 Mbps, Duplex: full
# Slave Interface: eth1
# Speed: 100 Mbps, Duplex: half    ← ¡Diferente!

# En modo 802.3ad, LACP requiere misma velocidad/duplex
# Los enlaces con speed/duplex diferente forman
# aggregators separados → no se combinan

CORRECTO:
# Verificar que ambos puertos negocian la misma velocidad
ethtool eth0 | grep -E "Speed|Duplex"
ethtool eth1 | grep -E "Speed|Duplex"

# Si un puerto negocia a 100 Mbps, verificar:
# - Cable Cat5e o superior
# - Puerto del switch habilitado para gigabit
# - Autonegociación activa en ambos lados
```

### Error 4: Failover lento por no configurar updelay/downdelay

```
PROBLEMA:
# miimon=100 pero sin updelay
# El enlace que vuelve se activa inmediatamente
# Si flapping (cable flojo), el bond oscila rápidamente:
# eth0 UP → eth0 DOWN → eth0 UP → eth0 DOWN...

CORRECTO:
# Configurar updelay para evitar flapping
# Esperar 200ms después de link-up antes de activar
echo 200 > /sys/class/net/bond0/bonding/updelay

# En NetworkManager:
bond.options "mode=active-backup,miimon=100,updelay=200,downdelay=0"

# updelay debe ser múltiplo de miimon
# 200ms updelay con miimon=100 → 2 checks antes de activar
```

### Error 5: Esperar que bonding duplique la velocidad de una conexión individual

```
INCORRECTO:
"Tengo bond0 con 2x 1Gbps = 2Gbps
 Mi descarga de un archivo debe ir a 2Gbps"

CORRECTO:
Un FLUJO individual (una conexión TCP) siempre pasa
por UN solo enlace → máximo 1Gbps.

Lo que el bonding duplica es el ancho de banda AGREGADO:
  Flujo 1 (descarga archivo A): 1 Gbps → eth0
  Flujo 2 (descarga archivo B): 1 Gbps → eth1
  Total simultáneo: 2 Gbps

Para acelerar un flujo individual necesitas:
  - Una interfaz más rápida (10GbE, 25GbE)
  - O multipath a nivel de aplicación (iSCSI multipath, NFS nconnect)
```

---

## 12. Cheatsheet

```
MODOS
──────────────────────────────────────────────
0  balance-rr       Round-robin        Switch: SÍ
1  active-backup    Solo 1 activo      Switch: NO
2  balance-xor      Hash MAC           Switch: SÍ
3  broadcast        Todas copian       Switch: SÍ
4  802.3ad          LACP negociado     Switch: LACP
5  balance-tlb      TX balanceado      Switch: NO
6  balance-alb      TX+RX balanceado   Switch: NO

RÁPIDA DECISIÓN
──────────────────────────────────────────────
Switch con LACP  → modo 4 (802.3ad)
Switch sin LACP  → modo 1 (active-backup) o modo 6 (balance-alb)
Solo redundancia → modo 1 (active-backup)

PARÁMETROS CLAVE
──────────────────────────────────────────────
mode              Modo de bonding
miimon            Intervalo MII monitor (ms, típico: 100)
updelay           Espera tras link-up (ms, típico: 200)
downdelay         Espera tras link-down (ms, típico: 0)
lacp_rate         fast (1s) o slow (30s)
xmit_hash_policy  layer2, layer3+4, layer2+3
primary           Slave preferido (active-backup)

NETWORKMANAGER
──────────────────────────────────────────────
nmcli con add type bond con-name bond0 ifname bond0 \
    bond.options "mode=802.3ad,miimon=100,lacp_rate=fast"
nmcli con add type ethernet con-name bond0-eth0 \
    ifname eth0 master bond0

SYSTEMD-NETWORKD
──────────────────────────────────────────────
.netdev:                 .network (slaves):
[NetDev]                 [Match]
Name=bond0               Name=eth0 eth1
Kind=bond                [Network]
[Bond]                   Bond=bond0
Mode=802.3ad
MIIMonitorSec=100ms

DIAGNÓSTICO
──────────────────────────────────────────────
cat /proc/net/bonding/bond0       Estado completo
cat /sys/class/net/bond0/bonding/mode    Modo
cat /sys/class/net/bond0/bonding/slaves  Slaves
ip -s link show eth0              Tráfico por slave
dmesg | grep bond                 Eventos kernel
journalctl -k | grep bond        Eventos kernel (systemd)

VERIFICAR LACP
──────────────────────────────────────────────
Partner Mac Address != 00:00:00:00:00:00  → LACP negociado
Number of ports = N slaves                → Todos agregados
Aggregator ID = mismo en todos            → Misma agregación
```

---

## 13. Ejercicios

### Ejercicio 1: Elegir el modo correcto

Para cada escenario, indica qué modo de bonding usarías y por qué:

1. Un servidor de base de datos conectado a un switch doméstico (TP-Link sin LACP). La prioridad es que no pierda conectividad si falla un cable.
2. Un servidor NFS en un data center conectado a un switch Cisco Catalyst con LACP. 50 clientes acceden simultáneamente.
3. Un servidor de monitorización conectado a dos switches diferentes (para redundancia si un switch falla). Cada interfaz va a un switch distinto.
4. Un servidor en una red donde el administrador de red se niega a configurar nada en los switches, pero quieres maximizar el ancho de banda de salida.

### Ejercicio 2: Interpretar el estado del bonding

Dado esta salida de `/proc/net/bonding/bond0`:

```
Bonding Mode: IEEE 802.3ad Dynamic link aggregation
Transmit Hash Policy: layer2 (0)
MII Status: up
MII Polling Interval (ms): 100
Up Delay (ms): 0
Down Delay (ms): 0

802.3ad info
LACP active: on
LACP rate: slow
Aggregator selection policy (ad_select): stable

Slave Interface: eth0
MII Status: up
Speed: 1000 Mbps
Duplex: full
Link Failure Count: 12
Aggregator ID: 1

Slave Interface: eth1
MII Status: up
Speed: 1000 Mbps
Duplex: full
Link Failure Count: 0
Aggregator ID: 1

Slave Interface: eth2
MII Status: up
Speed: 100 Mbps
Duplex: full
Link Failure Count: 0
Aggregator ID: 2
```

1. ¿Cuántas interfaces están en el bond? ¿Todas están participando en la misma agregación?
2. eth2 tiene Aggregator ID diferente. ¿Por qué? ¿Qué consecuencia tiene?
3. eth0 tiene 12 link failures. ¿Qué indica esto? ¿Qué investigarías?
4. La hash policy es `layer2`. ¿Es óptima si los 50 clientes acceden a este servidor a través de un único router (misma MAC destino)?
5. El LACP rate es `slow`. ¿Cuánto tardará en detectar una falla? ¿Cómo mejorarías la detección?
6. No hay `updelay`. ¿Qué problema puede causar esto combinado con los 12 link failures de eth0?

### Ejercicio 3: Configurar y diagnosticar

En una VM con dos interfaces (eth0, eth1):

1. Configura un bond en modo active-backup con miimon=100 usando tu gestor de red preferido.
2. Verifica con `/proc/net/bonding/bond0` que ambos slaves están UP y cuál es el activo.
3. Simula un fallo: desactiva el slave activo con `ip link set ethX down`.
4. Verifica en los logs que ocurrió el failover. ¿Cuánto tardó?
5. Reactiva el slave. ¿Volvió a ser el activo? ¿Por qué sí o por qué no? (Investiga el parámetro `primary_reselect`)
6. Cambia el modo a balance-alb y repite. ¿Qué diferencias observas en `/proc/net/bonding/bond0`?

**Pregunta de reflexión**: Un servidor tiene un bond con 2x 10GbE en modo 802.3ad y sirve como NFS server. Un usuario se queja de que "solo transfiere a 10Gbps, no a 20Gbps" cuando copia un archivo grande. ¿Tiene razón en quejarse? ¿Cómo le explicarías el comportamiento? ¿Existe alguna forma de que un solo flujo NFS aproveche ambos enlaces? (Pista: investiga `nconnect` en NFSv4.1+)

---

> **Siguiente tema**: T03 — VLANs (802.1Q, configuración con ip link, NetworkManager)
