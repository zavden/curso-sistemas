# T03 — VLANs: 802.1Q en Linux

## Índice

1. [¿Qué es una VLAN?](#1-qué-es-una-vlan)
2. [802.1Q: el estándar de VLAN tagging](#2-8021q-el-estándar-de-vlan-tagging)
3. [Puertos access vs trunk](#3-puertos-access-vs-trunk)
4. [VLANs en Linux: conceptos](#4-vlans-en-linux-conceptos)
5. [Configurar VLANs con ip link](#5-configurar-vlans-con-ip-link)
6. [Configurar VLANs con NetworkManager](#6-configurar-vlans-con-networkmanager)
7. [Configurar VLANs con systemd-networkd](#7-configurar-vlans-con-systemd-networkd)
8. [Configurar VLANs con ifupdown](#8-configurar-vlans-con-ifupdown)
9. [VLANs sobre bonds](#9-vlans-sobre-bonds)
10. [VLANs en virtualización y contenedores](#10-vlans-en-virtualización-y-contenedores)
11. [Diagnóstico](#11-diagnóstico)
12. [Errores comunes](#12-errores-comunes)
13. [Cheatsheet](#13-cheatsheet)
14. [Ejercicios](#14-ejercicios)

---

## 1. ¿Qué es una VLAN?

Una VLAN (Virtual LAN) divide una red física en múltiples redes lógicas aisladas. Dispositivos en la misma VLAN pueden comunicarse entre sí como si estuvieran en el mismo switch, aunque estén en switches diferentes. Dispositivos en VLANs distintas **no pueden** comunicarse directamente — necesitan un router.

```
Sin VLANs — un switch, una red:

┌──────────────────────────────────────────────┐
│                 Switch                        │
│                                              │
│  Puerto 1   Puerto 2   Puerto 3   Puerto 4  │
│     │           │           │          │     │
└─────┼───────────┼───────────┼──────────┼─────┘
      │           │           │          │
   Contab.     RRHH        Contab.    Servidor
   PC-A        PC-B        PC-C       FTP

Todos en la misma red → PC-B (RRHH) puede ver
el tráfico del servidor FTP de contabilidad

Con VLANs — un switch, dos redes aisladas:

┌──────────────────────────────────────────────┐
│                 Switch                        │
│                                              │
│  Puerto 1   Puerto 2   Puerto 3   Puerto 4  │
│  VLAN 10    VLAN 20    VLAN 10    VLAN 10   │
│     │           │           │          │     │
└─────┼───────────┼───────────┼──────────┼─────┘
      │           │           │          │
   Contab.     RRHH        Contab.    Servidor
   PC-A        PC-B        PC-C       FTP

VLAN 10 (Contabilidad): PC-A, PC-C, FTP → se ven entre sí
VLAN 20 (RRHH): PC-B → aislado de VLAN 10
PC-B NO puede acceder al servidor FTP
```

**¿Por qué usar VLANs?**

| Beneficio | Explicación |
|-----------|-------------|
| Seguridad | Aislar departamentos, servidores, invitados |
| Reducir broadcast | Los broadcasts se contienen dentro de la VLAN |
| Flexibilidad | Mover un equipo de VLAN sin cambiar cableado |
| Organización | Segmentar la red lógicamente sin hardware adicional |
| Cumplimiento | PCI-DSS requiere segmentación de redes con datos de tarjetas |

---

## 2. 802.1Q: el estándar de VLAN tagging

IEEE 802.1Q define cómo marcar (tag) las tramas Ethernet para indicar a qué VLAN pertenecen. Se inserta un campo de 4 bytes en el header Ethernet:

```
Trama Ethernet normal:
┌──────────┬──────────┬──────────┬─────────────────┬─────┐
│ Dest MAC │ Src MAC  │ EtherType│    Payload       │ FCS │
│  6 bytes │  6 bytes │  2 bytes │   46-1500 bytes  │ 4 B │
└──────────┴──────────┴──────────┴─────────────────┴─────┘

Trama 802.1Q (con VLAN tag):
┌──────────┬──────────┬──────┬──────┬──────────┬──────────────┬─────┐
│ Dest MAC │ Src MAC  │ TPID │ TCI  │ EtherType│   Payload    │ FCS │
│  6 bytes │  6 bytes │ 2 B  │ 2 B  │  2 bytes │ 46-1500 bytes│ 4 B │
└──────────┴──────────┴──────┴──────┴──────────┴──────────────┴─────┘
                       │      │
                       │      └── Tag Control Information
                       │          ├── PCP (3 bits): Priority (QoS)
                       │          ├── DEI (1 bit): Drop Eligible
                       │          └── VID (12 bits): VLAN ID
                       │
                       └── Tag Protocol Identifier
                           Siempre 0x8100 (indica "esta trama tiene VLAN tag")
```

El campo **VID** (VLAN Identifier) tiene 12 bits, lo que permite:
- VLAN 0: reservada (priority tagging)
- VLAN 1: VLAN nativa por defecto en la mayoría de switches
- VLAN 2-4094: VLANs utilizables
- VLAN 4095: reservada

Total: **4094 VLANs posibles** (2-4094 utilizables).

### MTU y VLANs

El tag 802.1Q añade 4 bytes a la trama. Una trama estándar de 1518 bytes pasa a 1522 bytes con tag. Los switches modernos manejan esto automáticamente (baby giant frames), pero en equipos antiguos puede causar problemas:

```
Sin tag:  1500 (payload) + 14 (header) + 4 (FCS) = 1518 bytes
Con tag:  1500 (payload) + 14 (header) + 4 (tag) + 4 (FCS) = 1522 bytes

En Linux, la interfaz VLAN hereda el MTU de la interfaz padre.
Si eth0 tiene MTU 1500, eth0.100 también tendrá MTU 1500.
El tag 802.1Q es transparente a nivel de MTU en Linux.
```

---

## 3. Puertos access vs trunk

Los puertos del switch se configuran en uno de dos modos respecto a VLANs:

```
Puerto ACCESS:
  - Pertenece a UNA sola VLAN
  - Las tramas entran y salen SIN tag 802.1Q
  - El dispositivo conectado no sabe que está en una VLAN
  - Uso: PCs, impresoras, teléfonos

Puerto TRUNK:
  - Transporta MÚLTIPLES VLANs simultáneamente
  - Las tramas llevan el tag 802.1Q (excepto la VLAN nativa)
  - El dispositivo conectado debe entender 802.1Q
  - Uso: entre switches, hacia servidores/routers
```

```
Ejemplo: dos switches conectados por trunk

Switch A                    Trunk (802.1Q)              Switch B
┌──────────┐              ┌──────────────┐             ┌──────────┐
│ P1: V10  │  ┌───────────┤  VLAN 10 tag ├────────────┐│ P1: V10  │
│ P2: V20  ├──┤ Trunk     │  VLAN 20 tag │   Trunk    ├┤ P2: V20  │
│ P3: V10  │  └───────────┤  VLAN 30 tag ├────────────┘│ P3: V30  │
└──────────┘              └──────────────┘             └──────────┘

PC en V10 de Switch A puede hablar con PC en V10 de Switch B
porque el trunk transporta VLAN 10 entre ambos switches.
```

### VLAN nativa (native VLAN)

En un trunk, las tramas de la VLAN nativa viajan **sin tag**. Por defecto es la VLAN 1:

```
Trunk con native VLAN 1:

Trama de VLAN 10: → sale con tag (VID=10)
Trama de VLAN 20: → sale con tag (VID=20)
Trama de VLAN 1:  → sale SIN tag (es la nativa)
```

> **Seguridad**: la VLAN nativa es un vector de ataque (VLAN hopping). Las mejores prácticas recomiendan cambiar la VLAN nativa a una VLAN no utilizada y nunca poner dispositivos en la VLAN 1.

---

## 4. VLANs en Linux: conceptos

Cuando un servidor Linux se conecta a un puerto trunk, necesita crear **sub-interfaces VLAN** para participar en cada VLAN:

```
Servidor Linux conectado a trunk:

┌─────────────────────────────────────┐
│              Linux                   │
│                                     │
│  eth0  ← Interfaz física            │
│  ├── eth0.10  ← Subinterfaz VLAN 10 │
│  │   IP: 10.10.0.5/24              │
│  ├── eth0.20  ← Subinterfaz VLAN 20 │
│  │   IP: 10.20.0.5/24              │
│  └── eth0.30  ← Subinterfaz VLAN 30 │
│      IP: 10.30.0.5/24              │
│                                     │
└──────────────┬──────────────────────┘
               │ Puerto trunk (802.1Q)
               │
         ┌─────┴─────┐
         │   Switch   │
         │ V10,V20,V30│
         └────────────┘
```

El módulo del kernel `8021q` maneja la encapsulación/desencapsulación de los tags. Cuando una trama llega a eth0 con tag VID=10, el kernel la entrega a eth0.10. Cuando eth0.10 envía una trama, el kernel le añade el tag VID=10 antes de enviarla por eth0.

```bash
# Verificar que el módulo está cargado
lsmod | grep 8021q

# Cargarlo si no está
sudo modprobe 8021q
```

### Nomenclatura de interfaces VLAN

Linux permite varios formatos de nombre:

```
eth0.100        ← Convención más común (interfaz.vlanID)
vlan100         ← Nombre descriptivo
eth0-vlan100    ← Con guión
v100-mgmt       ← Cualquier nombre (con ip link add ... name)
```

El formato `interfaz.ID` es solo una convención. Lo que realmente define el VLAN ID es el parámetro `id` al crear la interfaz.

---

## 5. Configurar VLANs con ip link

### Crear y configurar

```bash
# Asegurar que el módulo está cargado
sudo modprobe 8021q

# Crear interfaz VLAN (eth0 debe estar UP)
sudo ip link add link eth0 name eth0.100 type vlan id 100

# Subir la interfaz
sudo ip link set eth0.100 up

# Asignar IP
sudo ip addr add 10.100.0.5/24 dev eth0.100

# Gateway (si es necesario)
sudo ip route add default via 10.100.0.1 dev eth0.100
```

### Con nombre personalizado

```bash
# Nombre descriptivo en lugar de eth0.100
sudo ip link add link eth0 name vlan-mgmt type vlan id 100
sudo ip link set vlan-mgmt up
sudo ip addr add 10.100.0.5/24 dev vlan-mgmt
```

### Múltiples VLANs

```bash
# Crear varias VLANs sobre la misma interfaz física
for vid in 10 20 30; do
    sudo ip link add link eth0 name eth0.${vid} type vlan id ${vid}
    sudo ip link set eth0.${vid} up
done

sudo ip addr add 10.10.0.5/24 dev eth0.10
sudo ip addr add 10.20.0.5/24 dev eth0.20
sudo ip addr add 10.30.0.5/24 dev eth0.30
```

### Eliminar

```bash
sudo ip link del eth0.100
```

### Ver información de una VLAN

```bash
# Detalles de la interfaz VLAN
ip -d link show eth0.100
```

```
4: eth0.100@eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc noqueue state UP
    link/ether aa:bb:cc:11:22:33 brd ff:ff:ff:ff:ff:ff
    vlan protocol 802.1Q id 100 <REORDER_HDR>
```

Observa:
- `eth0.100@eth0` — indica que está vinculada a eth0
- `vlan protocol 802.1Q id 100` — confirma VLAN ID 100
- La MAC es la misma que eth0 (la hereda)

---

## 6. Configurar VLANs con NetworkManager

### Crear VLAN

```bash
# VLAN con DHCP
nmcli connection add type vlan con-name vlan100 \
    vlan.parent eth0 \
    vlan.id 100 \
    ipv4.method auto

# VLAN con IP estática
nmcli connection add type vlan con-name vlan-mgmt \
    vlan.parent eth0 \
    vlan.id 100 \
    ipv4.method manual \
    ipv4.addresses "10.100.0.5/24" \
    ipv4.gateway "10.100.0.1" \
    ipv4.dns "10.100.0.1"

# Activar
nmcli connection up vlan100
```

### Nombre de la interfaz

Por defecto, NetworkManager nombra la interfaz VLAN como `eth0.100`. Se puede personalizar:

```bash
# Nombre personalizado
nmcli connection add type vlan con-name vlan-mgmt \
    vlan.parent eth0 \
    vlan.id 100 \
    ifname vlan-mgmt \
    ipv4.method manual \
    ipv4.addresses "10.100.0.5/24"
```

### Múltiples VLANs

```bash
nmcli connection add type vlan con-name vlan10 \
    vlan.parent eth0 vlan.id 10 \
    ipv4.method manual ipv4.addresses "10.10.0.5/24"

nmcli connection add type vlan con-name vlan20 \
    vlan.parent eth0 vlan.id 20 \
    ipv4.method manual ipv4.addresses "10.20.0.5/24"

nmcli connection add type vlan con-name vlan30 \
    vlan.parent eth0 vlan.id 30 \
    ipv4.method manual ipv4.addresses "10.30.0.5/24"

nmcli connection up vlan10
nmcli connection up vlan20
nmcli connection up vlan30
```

### Verificar

```bash
# Ver conexiones VLAN
nmcli connection show | grep vlan

# Detalles
nmcli connection show vlan100

# Dispositivos
nmcli device status
```

### Archivo de perfil resultante

```ini
# /etc/NetworkManager/system-connections/vlan100.nmconnection
[connection]
id=vlan100
type=vlan
interface-name=eth0.100

[vlan]
id=100
parent=eth0

[ipv4]
method=manual
address1=10.100.0.5/24,10.100.0.1

[ipv6]
method=disabled
```

---

## 7. Configurar VLANs con systemd-networkd

### Crear la VLAN

```ini
# /etc/systemd/network/20-vlan100.netdev
[NetDev]
Name=eth0.100
Kind=vlan

[VLAN]
Id=100
```

### Configurar la interfaz padre

```ini
# /etc/systemd/network/25-eth0.network
[Match]
Name=eth0

[Network]
# La interfaz padre puede tener su propia IP (VLAN nativa)
# o no (solo transporta VLANs)
VLAN=eth0.100
VLAN=eth0.200
VLAN=eth0.300
```

### Configurar la VLAN

```ini
# /etc/systemd/network/30-vlan100.network
[Match]
Name=eth0.100

[Network]
Address=10.100.0.5/24
Gateway=10.100.0.1
DNS=10.100.0.1
```

### Ejemplo completo: servidor con 3 VLANs

```ini
# /etc/systemd/network/20-vlan10.netdev
[NetDev]
Name=eth0.10
Kind=vlan
[VLAN]
Id=10
```

```ini
# /etc/systemd/network/20-vlan20.netdev
[NetDev]
Name=eth0.20
Kind=vlan
[VLAN]
Id=20
```

```ini
# /etc/systemd/network/20-vlan30.netdev
[NetDev]
Name=eth0.30
Kind=vlan
[VLAN]
Id=30
```

```ini
# /etc/systemd/network/25-eth0.network
[Match]
Name=eth0

[Network]
VLAN=eth0.10
VLAN=eth0.20
VLAN=eth0.30
# eth0 no tiene IP propia (solo trunk)
```

```ini
# /etc/systemd/network/30-vlan10.network
[Match]
Name=eth0.10
[Network]
Address=10.10.0.5/24
DNS=10.10.0.53
Domains=~prod.internal
```

```ini
# /etc/systemd/network/30-vlan20.network
[Match]
Name=eth0.20
[Network]
Address=10.20.0.5/24
DNS=10.20.0.53
Domains=~dev.internal
```

```ini
# /etc/systemd/network/30-vlan30.network
[Match]
Name=eth0.30
[Network]
Address=10.30.0.5/24
Gateway=10.30.0.1
DNS=8.8.8.8
Domains=~.
```

```bash
# Aplicar
sudo networkctl reload
sudo networkctl reconfigure eth0
```

---

## 8. Configurar VLANs con ifupdown

Requiere el paquete `vlan`:

```bash
sudo apt install vlan
```

### Configuración básica

```
# /etc/network/interfaces

# Interfaz física (sin IP si solo hace trunk)
auto eth0
iface eth0 inet manual

# VLAN 100
auto eth0.100
iface eth0.100 inet static
    address 10.100.0.5/24
    gateway 10.100.0.1
    dns-nameservers 10.100.0.1
    vlan-raw-device eth0
```

### Múltiples VLANs

```
auto eth0
iface eth0 inet manual

auto eth0.10
iface eth0.10 inet static
    address 10.10.0.5/24
    vlan-raw-device eth0

auto eth0.20
iface eth0.20 inet static
    address 10.20.0.5/24
    vlan-raw-device eth0

auto eth0.30
iface eth0.30 inet dhcp
    vlan-raw-device eth0
```

La directiva `vlan-raw-device` es técnicamente opcional si el nombre sigue el formato `interfaz.id` — ifupdown lo infiere automáticamente. Pero es buena práctica incluirla para claridad.

### Interfaz padre con IP (VLAN nativa)

```
# eth0 participa en la VLAN nativa (sin tag) Y tiene VLANs tagged
auto eth0
iface eth0 inet static
    address 192.168.1.50/24
    gateway 192.168.1.1

auto eth0.100
iface eth0.100 inet static
    address 10.100.0.5/24
```

---

## 9. VLANs sobre bonds

Un escenario muy común en data centers: bonding para redundancia y VLANs para segmentación, combinados:

```
┌──────────────────────────────────────────┐
│                Linux                      │
│                                          │
│  bond0  ← Bond (2x 10GbE)               │
│  ├── bond0.10  ← VLAN 10 (Producción)   │
│  │   IP: 10.10.0.5/24                   │
│  ├── bond0.20  ← VLAN 20 (Desarrollo)   │
│  │   IP: 10.20.0.5/24                   │
│  └── bond0.30  ← VLAN 30 (Management)   │
│      IP: 10.30.0.5/24                   │
│                                          │
│  eth0 ─┐                                │
│         ├── bond0 (802.3ad)              │
│  eth1 ─┘                                │
└──────────────┬───────────────────────────┘
               │ Trunk 802.1Q sobre LACP
         ┌─────┴─────┐
         │   Switch   │
         │  LACP +    │
         │  VLAN trunk│
         └────────────┘
```

### Con ip link

```bash
# 1. Crear bond
sudo ip link add bond0 type bond mode 802.3ad miimon 100
sudo ip link set eth0 down && sudo ip link set eth0 master bond0
sudo ip link set eth1 down && sudo ip link set eth1 master bond0
sudo ip link set bond0 up

# 2. Crear VLANs sobre el bond
sudo ip link add link bond0 name bond0.10 type vlan id 10
sudo ip link add link bond0 name bond0.20 type vlan id 20
sudo ip link set bond0.10 up
sudo ip link set bond0.20 up

# 3. Asignar IPs a las VLANs (no al bond)
sudo ip addr add 10.10.0.5/24 dev bond0.10
sudo ip addr add 10.20.0.5/24 dev bond0.20
```

### Con NetworkManager

```bash
# Bond
nmcli con add type bond con-name bond0 ifname bond0 \
    bond.options "mode=802.3ad,miimon=100,lacp_rate=fast"
nmcli con add type ethernet con-name bond0-eth0 ifname eth0 master bond0
nmcli con add type ethernet con-name bond0-eth1 ifname eth1 master bond0

# VLANs sobre el bond
nmcli con add type vlan con-name vlan10 vlan.parent bond0 vlan.id 10 \
    ipv4.method manual ipv4.addresses "10.10.0.5/24"
nmcli con add type vlan con-name vlan20 vlan.parent bond0 vlan.id 20 \
    ipv4.method manual ipv4.addresses "10.20.0.5/24"

nmcli con up bond0
nmcli con up vlan10
nmcli con up vlan20
```

### Con systemd-networkd

```ini
# /etc/systemd/network/20-bond0.netdev
[NetDev]
Name=bond0
Kind=bond
[Bond]
Mode=802.3ad
MIIMonitorSec=100ms
LACPTransmitRate=fast
```

```ini
# /etc/systemd/network/20-vlan10.netdev
[NetDev]
Name=bond0.10
Kind=vlan
[VLAN]
Id=10
```

```ini
# /etc/systemd/network/20-vlan20.netdev
[NetDev]
Name=bond0.20
Kind=vlan
[VLAN]
Id=20
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
VLAN=bond0.10
VLAN=bond0.20
# bond0 sin IP propia
```

```ini
# /etc/systemd/network/35-vlan10.network
[Match]
Name=bond0.10
[Network]
Address=10.10.0.5/24
Gateway=10.10.0.1
```

```ini
# /etc/systemd/network/35-vlan20.network
[Match]
Name=bond0.20
[Network]
Address=10.20.0.5/24
```

---

## 10. VLANs en virtualización y contenedores

### VLANs con KVM/libvirt

Un hipervisor puede exponer VLANs a las máquinas virtuales de dos formas:

```
Opción 1: Bridge por VLAN (más simple)

Host:
  eth0 (trunk)
  ├── eth0.10 ──► br-prod ──► VM-1 (ve una red normal)
  └── eth0.20 ──► br-dev  ──► VM-2 (ve una red normal)

Las VMs no saben que están en una VLAN.
Cada VLAN tiene su propio bridge.

Opción 2: Trunk al VM (VM hace el tagging)

Host:
  eth0 (trunk) ──► br-trunk ──► VM-1 (recibe tramas tagged)
                                └── eth0.10 (dentro de la VM)
                                └── eth0.20 (dentro de la VM)

La VM gestiona las VLANs internamente.
Más flexible pero más complejo.
```

### VLANs con Docker

```bash
# Crear red Docker en una VLAN específica
# Requiere macvlan o ipvlan driver

# Macvlan: cada contenedor tiene su propia MAC en la VLAN
docker network create -d macvlan \
    --subnet=10.100.0.0/24 \
    --gateway=10.100.0.1 \
    -o parent=eth0.100 \
    vlan100-net

# Usar la red
docker run -d --network vlan100-net --ip 10.100.0.50 nginx

# IPvlan: contenedores comparten la MAC del host
docker network create -d ipvlan \
    --subnet=10.100.0.0/24 \
    --gateway=10.100.0.1 \
    -o parent=eth0.100 \
    -o ipvlan_mode=l2 \
    vlan100-ipvlan
```

```
Docker macvlan sobre VLAN:

┌────────────────────────────────────────────┐
│                   Host                      │
│                                            │
│  eth0 (trunk)                              │
│  └── eth0.100                              │
│      └── macvlan bridge                    │
│          ├── Container A: 10.100.0.50      │
│          └── Container B: 10.100.0.51      │
│                                            │
│  Containers están directamente en VLAN 100 │
│  Accesibles desde otros hosts en VLAN 100  │
└────────────────────────────────────────────┘
```

---

## 11. Diagnóstico

### Verificar la configuración VLAN

```bash
# Ver todas las interfaces VLAN
ip -d link show type vlan
```

```
4: eth0.100@eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 ...
    vlan protocol 802.1Q id 100 <REORDER_HDR>
5: eth0.200@eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 ...
    vlan protocol 802.1Q id 200 <REORDER_HDR>
```

```bash
# Información detallada de una VLAN
cat /proc/net/vlan/eth0.100
```

```
eth0.100  VID: 100     REORDER_HDR: 1  dev->priv_flags: 1001
         total frames received          154321
          total bytes received        12345678
      Broadcast/Multicast Rcvd            1234

         total frames transmitted         98765
          total bytes transmitted        7654321
Device: eth0
INGRESS priority mappings: 0:0  1:0  2:0  3:0  4:0  5:0  6:0 7:0
 EGRESS priority mappings:
```

```bash
# Listar todas las VLANs
ls /proc/net/vlan/

# Ver estadísticas
cat /proc/net/vlan/config
```

### Capturar tráfico VLAN

```bash
# Capturar en la interfaz física (ve los tags)
sudo tcpdump -i eth0 -e -nn vlan 100

# Capturar en la interfaz VLAN (ya desencapsulado)
sudo tcpdump -i eth0.100 -nn

# Ver tramas con tag (filtro por VLAN ID)
sudo tcpdump -i eth0 -e -nn 'vlan 100 and icmp'
```

### Verificar conectividad por VLAN

```bash
# Ping usando la interfaz VLAN específica
ping -I eth0.100 10.100.0.1

# Verificar que la ruta usa la interfaz correcta
ip route get 10.100.0.1
# 10.100.0.1 dev eth0.100 src 10.100.0.5

# Ver rutas por interfaz VLAN
ip route show dev eth0.100

# ARP en la VLAN
ip neigh show dev eth0.100
```

### Verificar que el trunk funciona

```bash
# Si no llegan tramas tagged, verificar:

# 1. ¿La interfaz física está UP?
ip link show eth0

# 2. ¿El módulo 8021q está cargado?
lsmod | grep 8021q

# 3. ¿Llegan tramas con tags? (capturar en raw)
sudo tcpdump -i eth0 -e -nn | grep "vlan"

# 4. ¿El switch tiene configurado trunk en este puerto?
# (verificar en el switch)
```

---

## 12. Errores comunes

### Error 1: Puerto del switch en modo access en vez de trunk

```
PROBLEMA:
# Creaste eth0.100 en Linux
# Pero el puerto del switch está en modo access VLAN 1
# → Las tramas con tag VLAN 100 se descartan

# Síntoma: ping a cualquier IP en VLAN 100 falla
# tcpdump en eth0 muestra paquetes saliendo pero sin respuesta

SOLUCIÓN:
# Configurar el puerto del switch como trunk
# permitiendo VLAN 100:

# Cisco:
# interface GigabitEthernet0/1
#   switchport mode trunk
#   switchport trunk allowed vlan 10,20,100

# Si no tienes acceso al switch, contacta al equipo de redes
```

### Error 2: Crear la VLAN con la interfaz padre DOWN

```
PROBLEMA:
sudo ip link add link eth0 name eth0.100 type vlan id 100
sudo ip link set eth0.100 up
sudo ip addr add 10.100.0.5/24 dev eth0.100
# No funciona

ip link show eth0
# eth0: <BROADCAST,MULTICAST> ← ¡Falta UP y LOWER_UP!

CORRECTO:
# La interfaz padre DEBE estar UP
sudo ip link set eth0 up
sudo ip link add link eth0 name eth0.100 type vlan id 100
sudo ip link set eth0.100 up
sudo ip addr add 10.100.0.5/24 dev eth0.100
```

### Error 3: Misma subred en la interfaz padre y la VLAN

```
INCORRECTO:
# eth0 en la VLAN nativa
ip addr add 192.168.1.50/24 dev eth0

# eth0.100 con IP en la MISMA subred
ip addr add 192.168.1.51/24 dev eth0.100

# El kernel no sabe por qué interfaz enviar tráfico a 192.168.1.x
# Resultado: comportamiento impredecible, paquetes que van
# por la interfaz equivocada

CORRECTO:
# Cada VLAN debe tener su propia subred
ip addr add 192.168.1.50/24 dev eth0         # Subred 192.168.1.0/24
ip addr add 10.100.0.50/24  dev eth0.100     # Subred 10.100.0.0/24
```

### Error 4: Olvidar que la VLAN hereda el MTU del padre

```
PROBLEMA:
# eth0 tiene MTU 1500
# Configuraste MTU 9000 en eth0.100
sudo ip link set eth0.100 mtu 9000
# Error: "Numerical result out of range"
# No puede exceder el MTU del padre

CORRECTO:
# Primero subir el MTU del padre
sudo ip link set eth0 mtu 9000
sudo ip link set eth0.100 mtu 9000

# Verificar que el switch también soporta jumbo frames
# en ese trunk
```

### Error 5: Confundir VLAN ID con subred

```
INCORRECTO:
# "VLAN 10 debe usar la subred 10.10.0.0/24
#  VLAN 20 debe usar la subred 10.20.0.0/24"

# Esto es una CONVENCIÓN, no un requisito técnico
# La VLAN ID y la subred son independientes

CORRECTO (pero aclara):
# VLAN ID y subred NO tienen relación técnica
# Puedes tener VLAN 10 con subred 172.16.0.0/24

# Sin embargo, la convención de alinear VLAN ID con subred
# (VLAN 10 → 10.10.0.0/24) es una BUENA PRÁCTICA:
# - Facilita el troubleshooting
# - Es predecible para el equipo
# - Se documenta sola

# Pero no la confundas con un requisito del protocolo
```

---

## 13. Cheatsheet

```
CREAR VLAN
──────────────────────────────────────────────
ip link:
  ip link add link eth0 name eth0.100 type vlan id 100
  ip link set eth0.100 up
  ip addr add 10.100.0.5/24 dev eth0.100

NetworkManager:
  nmcli con add type vlan con-name vlan100 \
      vlan.parent eth0 vlan.id 100 \
      ipv4.method manual ipv4.addresses "10.100.0.5/24"

systemd-networkd:
  .netdev:  [NetDev] Name=eth0.100  Kind=vlan
            [VLAN]   Id=100
  padre:    [Network] VLAN=eth0.100
  config:   [Match] Name=eth0.100
            [Network] Address=10.100.0.5/24

ifupdown:
  auto eth0.100
  iface eth0.100 inet static
      address 10.100.0.5/24
      vlan-raw-device eth0

ELIMINAR VLAN
──────────────────────────────────────────────
ip link del eth0.100
nmcli con delete vlan100

DIAGNÓSTICO
──────────────────────────────────────────────
ip -d link show type vlan       Listar VLANs con detalles
cat /proc/net/vlan/config       VLANs configuradas
cat /proc/net/vlan/eth0.100     Estadísticas de una VLAN
tcpdump -i eth0 -e vlan 100     Capturar tráfico VLAN
lsmod | grep 8021q              Verificar módulo cargado

RECORDAR
──────────────────────────────────────────────
• Interfaz padre debe estar UP
• Cada VLAN = subred distinta
• MTU de VLAN ≤ MTU del padre
• Puerto del switch debe ser trunk
• VLAN ID: 2-4094 utilizables
• Convención: eth0.VID (pero el nombre es libre)

VLAN SOBRE BOND
──────────────────────────────────────────────
1. Crear bond (bond0)
2. Agregar slaves al bond
3. Crear VLANs sobre bond0 (no sobre eth0/eth1)
4. Asignar IPs a las VLANs

bond0 ← bond (sin IP)
├── bond0.10 ← VLAN (con IP)
├── bond0.20 ← VLAN (con IP)
└── bond0.30 ← VLAN (con IP)
```

---

## 14. Ejercicios

### Ejercicio 1: Planificar VLANs para una oficina

Una oficina tiene un switch de 24 puertos y tres departamentos:

- **Desarrollo** (8 PCs): necesitan acceso a Internet y a los servidores internos
- **Finanzas** (5 PCs): necesitan acceso a Internet pero NO deben ver el tráfico de desarrollo
- **Servidores** (4 servidores): accesibles desde desarrollo y finanzas, pero aislados entre sí

Diseña:
1. Asignación de VLANs (ID, nombre, subred para cada una)
2. Qué puertos del switch serían access y cuáles trunk
3. Un servidor Linux necesita estar en las tres VLANs simultáneamente (servidor de monitorización). Escribe la configuración usando tu gestor de red preferido.
4. ¿Cómo se comunican desarrollo y finanzas con los servidores si están en VLANs diferentes?

### Ejercicio 2: Configurar y verificar una VLAN

En una VM con acceso a la interfaz de red:

1. Verifica que el módulo `8021q` está cargado.
2. Crea una interfaz VLAN con ID 42 sobre tu interfaz principal.
3. Asígnale la IP 10.42.0.1/24.
4. Verifica con `ip -d link show type vlan` que la VLAN está creada.
5. Verifica con `cat /proc/net/vlan/config` que aparece.
6. Ejecuta `ip route show dev <tu-vlan>`. ¿Qué ruta creó el kernel automáticamente?
7. Elimina la VLAN y confirma que desapareció.

### Ejercicio 3: Diagnosticar problema de VLAN

Un administrador configuró VLAN 100 en un servidor Linux:

```bash
$ ip -d link show eth0.100
4: eth0.100@eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 ...
    vlan protocol 802.1Q id 100

$ ip addr show eth0.100
4: eth0.100@eth0: ...
    inet 10.100.0.5/24 brd 10.100.0.255 scope global eth0.100

$ ping 10.100.0.1
PING 10.100.0.1 (10.100.0.1) 56(84) bytes of data.
--- 10.100.0.1 ping statistics ---
5 packets transmitted, 0 received, 100% packet loss
```

La interfaz está UP, la IP está configurada, pero no hay conectividad. Enumera al menos 5 posibles causas ordenadas de más probable a menos probable, y para cada una describe cómo la verificarías.

**Pregunta de reflexión**: Un arquitecto de red propone usar VLANs para aislar completamente el entorno de producción del de desarrollo en el mismo switch físico. Un auditor de seguridad responde que "VLANs no son un control de seguridad suficiente" y exige switches físicamente separados. ¿Quién tiene razón? Investiga el concepto de "VLAN hopping" (double tagging attack, switch spoofing) y evalúa si las VLANs proveen aislamiento suficiente para datos sensibles.

---

> **Siguiente tema**: T04 — Bridge networking (crear bridges, uso en virtualización y Docker)
