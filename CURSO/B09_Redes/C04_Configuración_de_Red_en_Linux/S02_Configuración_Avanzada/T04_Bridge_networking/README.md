# T04 — Bridge Networking: Crear Bridges, Uso en Virtualización y Docker

## Índice

1. [¿Qué es un bridge en Linux?](#1-qué-es-un-bridge-en-linux)
2. [Cómo funciona un bridge](#2-cómo-funciona-un-bridge)
3. [Spanning Tree Protocol (STP)](#3-spanning-tree-protocol-stp)
4. [Configurar bridges con ip link](#4-configurar-bridges-con-ip-link)
5. [Configurar bridges con NetworkManager](#5-configurar-bridges-con-networkmanager)
6. [Configurar bridges con systemd-networkd](#6-configurar-bridges-con-systemd-networkd)
7. [Configurar bridges con ifupdown](#7-configurar-bridges-con-ifupdown)
8. [Bridges en virtualización (KVM/libvirt)](#8-bridges-en-virtualización-kvmlibvirt)
9. [Bridges en Docker](#9-bridges-en-docker)
10. [Bridge vs macvlan vs ipvlan](#10-bridge-vs-macvlan-vs-ipvlan)
11. [Diagnóstico](#11-diagnóstico)
12. [Errores comunes](#12-errores-comunes)
13. [Cheatsheet](#13-cheatsheet)
14. [Ejercicios](#14-ejercicios)

---

## 1. ¿Qué es un bridge en Linux?

Un bridge (puente) en Linux es un **switch virtual** implementado en software dentro del kernel. Conecta múltiples interfaces de red a nivel de capa 2 (Ethernet), reenviando tramas entre ellas basándose en direcciones MAC, exactamente como lo haría un switch físico.

```
Switch físico:                    Bridge Linux:

┌──────────────┐                  ┌──────────────────────┐
│   Switch     │                  │     Kernel Linux     │
│              │                  │                      │
│  P1  P2  P3  │                  │  br0 (bridge)        │
│  │   │   │   │                  │  ├── eth0 (puerto 1) │
└──┼───┼───┼───┘                  │  ├── eth1 (puerto 2) │
   │   │   │                      │  ├── veth0 (VM)      │
  PC  PC  PC                      │  └── tap0 (VM)       │
                                  └──────────────────────┘

Misma función: reenviar tramas entre puertos
basándose en la tabla MAC (FDB)
```

**¿Para qué se usa?**

| Uso | Descripción |
|-----|-------------|
| Virtualización | Conectar VMs a la red física del host |
| Contenedores | Docker usa bridges para la red de contenedores |
| Network namespaces | Conectar namespaces entre sí y con el exterior |
| Routing entre interfaces | Unir dos interfaces físicas en una sola red L2 |
| Testing | Simular topologías de red complejas |

---

## 2. Cómo funciona un bridge

### Tabla de reenvío (FDB — Forwarding Database)

El bridge aprende qué MAC está en qué puerto observando las tramas que llegan:

```
1. Llega trama por eth0 con MAC origen AA:BB:CC:11:22:33
   → Bridge aprende: "MAC AA:BB:CC:11:22:33 está en eth0"

2. La trama tiene MAC destino DD:EE:FF:44:55:66
   → Bridge busca en su tabla FDB
   → ¿Conoce ese MAC?
     SÍ: reenvía solo por ese puerto (unicast)
     NO: reenvía por TODOS los puertos (flood)

3. Cuando DD:EE:FF:44:55:66 responde por eth1
   → Bridge aprende: "MAC DD:EE:FF:44:55:66 está en eth1"

4. Futuras tramas a DD:EE:FF:44:55:66 van solo por eth1
```

```
Tabla FDB del bridge:

MAC Address          Puerto    Aging
───────────────────  ────────  ─────
aa:bb:cc:11:22:33    eth0      120s
dd:ee:ff:44:55:66    eth1      85s
11:22:33:44:55:66    veth0     42s
(broadcast)          todos     -
```

### El bridge como interfaz de red

Un bridge puede tener su propia dirección IP. Esto permite que el host participe en la red del bridge:

```
Sin IP en el bridge:                 Con IP en el bridge:

br0 (switch puro)                    br0 (switch + participante)
├── eth0 → red física               ├── eth0 → red física
├── veth0 → VM                      ├── veth0 → VM
└── (br0 no tiene IP,               └── IP: 192.168.1.50/24
     el host no participa                (el host puede comunicarse
     en esta red)                         con la red y las VMs)
```

---

## 3. Spanning Tree Protocol (STP)

STP previene bucles en topologías con múltiples bridges o switches conectados entre sí. Un bucle a nivel L2 causa **tormentas de broadcast** que pueden tumbar la red en segundos.

```
Bucle sin STP:

Bridge A ──────── Bridge B
    │                 │
    └──── Bridge C ───┘

Una trama broadcast circula indefinidamente:
A → B → C → A → B → C → ... ← Tormenta de broadcast

Con STP:

Bridge A ──────── Bridge B
    │                 │
    └──── Bridge C ───┘
              ╳
         (puerto bloqueado)

STP detecta el bucle y bloquea uno de los puertos.
Si el enlace A-B falla, STP desbloquea A-C-B.
```

### STP en bridges Linux

```bash
# Ver estado STP de un bridge
ip -d link show br0 | grep -i stp

# Habilitar STP
sudo ip link set br0 type bridge stp_state 1

# Deshabilitar STP
sudo ip link set br0 type bridge stp_state 0

# Ver detalles STP
bridge link show

# O con brctl (legacy)
brctl showstp br0
```

**¿Cuándo habilitar STP?**

| Escenario | STP |
|-----------|-----|
| Bridge simple (1 interfaz + VMs) | No necesario |
| Múltiples bridges interconectados | **Sí** |
| Bridge conectado a switches con STP | **Sí** |
| Bridge solo para contenedores Docker | No necesario |
| Bridge de testing aislado | No necesario |

> **Forward delay**: cuando STP está habilitado, los puertos pasan por estados (Blocking → Listening → Learning → Forwarding) antes de reenviar tráfico. Esto causa un retraso de ~30 segundos al conectar un nuevo puerto. Puedes reducirlo:

```bash
# Reducir forward delay (default 15s, se pasa 2 veces = 30s)
sudo ip link set br0 type bridge forward_delay 400
# 400 centésimas de segundo = 4 segundos

# O deshabilitar STP si no hay riesgo de bucles
sudo ip link set br0 type bridge stp_state 0
# Sin STP, forward delay = 0 (inmediato)
```

---

## 4. Configurar bridges con ip link

### Bridge básico

```bash
# 1. Crear el bridge
sudo ip link add name br0 type bridge

# 2. Subir el bridge
sudo ip link set br0 up

# 3. Agregar interfaz física al bridge
# (la interfaz pierde su IP al unirse al bridge)
sudo ip link set eth0 master br0

# 4. Asignar IP al bridge (no a eth0)
sudo ip addr add 192.168.1.50/24 dev br0

# 5. Ruta por defecto vía el bridge
sudo ip route add default via 192.168.1.1 dev br0
```

### Propiedades del bridge

```bash
# Forward delay
sudo ip link set br0 type bridge forward_delay 0

# STP on/off
sudo ip link set br0 type bridge stp_state 0

# Aging time (tiempo antes de olvidar una MAC, default 300s)
sudo ip link set br0 type bridge ageing_time 30000
# (en centésimas de segundo: 30000 = 300s)

# VLAN filtering
sudo ip link set br0 type bridge vlan_filtering 1

# Ver todas las propiedades
ip -d link show br0
```

### Agregar y quitar puertos

```bash
# Agregar
sudo ip link set eth0 master br0
sudo ip link set eth1 master br0
sudo ip link set veth0 master br0

# Quitar
sudo ip link set eth0 nomaster

# Ver puertos del bridge
bridge link show
```

### Eliminar bridge

```bash
# Quitar todos los puertos primero
sudo ip link set eth0 nomaster

# Bajar y eliminar
sudo ip link set br0 down
sudo ip link del br0
```

---

## 5. Configurar bridges con NetworkManager

### Bridge con interfaz física

```bash
# Crear bridge
nmcli connection add type bridge con-name br0 ifname br0 \
    ipv4.method manual \
    ipv4.addresses "192.168.1.50/24" \
    ipv4.gateway "192.168.1.1" \
    ipv4.dns "192.168.1.1"

# Agregar eth0 al bridge
nmcli connection add type ethernet con-name br0-eth0 \
    ifname eth0 master br0

# Desactivar STP (opcional, acelera conexión)
nmcli connection modify br0 bridge.stp no

# Forward delay 0 (si STP está desactivado)
nmcli connection modify br0 bridge.forward-delay 0

# Activar
nmcli connection up br0
```

### Bridge con DHCP

```bash
nmcli connection add type bridge con-name br0 ifname br0 \
    ipv4.method auto

nmcli connection add type ethernet con-name br0-eth0 \
    ifname eth0 master br0

nmcli connection modify br0 bridge.stp no
nmcli connection up br0
```

### Verificar

```bash
nmcli connection show br0
nmcli device show br0
nmcli -f bridge connection show br0
```

---

## 6. Configurar bridges con systemd-networkd

### Archivos necesarios

```ini
# /etc/systemd/network/20-br0.netdev
[NetDev]
Name=br0
Kind=bridge

[Bridge]
STP=false
ForwardDelaySec=0
```

```ini
# /etc/systemd/network/25-br0-bind.network
# Agregar eth0 al bridge
[Match]
Name=eth0

[Network]
Bridge=br0
# eth0 no tiene IP propia
```

```ini
# /etc/systemd/network/30-br0.network
# Configurar IP del bridge
[Match]
Name=br0

[Network]
Address=192.168.1.50/24
Gateway=192.168.1.1
DNS=192.168.1.1
```

### Bridge con DHCP

```ini
# /etc/systemd/network/30-br0.network
[Match]
Name=br0

[Network]
DHCP=yes
```

### Múltiples interfaces en el bridge

```ini
# /etc/systemd/network/25-br0-members.network
[Match]
Name=eth0 eth1 eth2

[Network]
Bridge=br0
```

### Bridge con propiedades avanzadas

```ini
# /etc/systemd/network/20-br0.netdev
[NetDev]
Name=br0
Kind=bridge

[Bridge]
STP=true
ForwardDelaySec=4
HelloTimeSec=2
MaxAgeSec=20
AgeingTimeSec=300
Priority=32768
DefaultPVID=1
VLANFiltering=false
```

---

## 7. Configurar bridges con ifupdown

```
# /etc/network/interfaces

# eth0 sin IP (miembro del bridge)
auto eth0
iface eth0 inet manual

# Bridge con IP estática
auto br0
iface br0 inet static
    address 192.168.1.50/24
    gateway 192.168.1.1
    dns-nameservers 192.168.1.1

    bridge_ports eth0
    bridge_stp off
    bridge_fd 0
    bridge_maxwait 0
```

Con DHCP:

```
auto eth0
iface eth0 inet manual

auto br0
iface br0 inet dhcp
    bridge_ports eth0
    bridge_stp off
    bridge_fd 0
```

Con múltiples puertos:

```
auto br0
iface br0 inet static
    address 192.168.1.50/24
    gateway 192.168.1.1
    bridge_ports eth0 eth1
    bridge_stp on
    bridge_fd 4
```

Requiere `bridge-utils`:

```bash
sudo apt install bridge-utils
```

---

## 8. Bridges en virtualización (KVM/libvirt)

El uso principal de bridges en Linux es conectar máquinas virtuales a la red física del host. Sin un bridge, las VMs solo pueden comunicarse con el host (red NAT). Con un bridge, las VMs obtienen IPs en la misma red que el host, accesibles desde toda la red.

### Sin bridge (NAT — default de libvirt)

```
┌──────────────────────────────────────────────┐
│                    Host                       │
│                                              │
│  virbr0 (bridge NAT, 192.168.122.1)         │
│  ├── vnet0 → VM-1 (192.168.122.100)         │
│  └── vnet1 → VM-2 (192.168.122.101)         │
│                                              │
│  eth0 (192.168.1.50) ← Red real             │
│       │                                      │
│       │ NAT (iptables MASQUERADE)            │
│       │ VMs salen a Internet vía NAT         │
│       │ pero NO son accesibles desde la red  │
└───────┼──────────────────────────────────────┘
        │
   Red física (192.168.1.0/24)
```

### Con bridge (acceso directo a la red)

```
┌──────────────────────────────────────────────┐
│                    Host                       │
│                                              │
│  br0 (192.168.1.50)                          │
│  ├── eth0 ← Interfaz física                 │
│  ├── vnet0 → VM-1 (192.168.1.101)           │
│  └── vnet1 → VM-2 (192.168.1.102)           │
│                                              │
│  Las VMs tienen IPs en la red real           │
│  Son accesibles desde otros hosts            │
│  Obtienen IP por DHCP del router real        │
└───────┼──────────────────────────────────────┘
        │
   Red física (192.168.1.0/24)
   Otros hosts ven las VMs directamente
```

### Crear bridge para libvirt

```bash
# 1. Crear bridge con NetworkManager
nmcli con add type bridge con-name br0 ifname br0 \
    ipv4.method manual \
    ipv4.addresses "192.168.1.50/24" \
    ipv4.gateway "192.168.1.1" \
    ipv4.dns "192.168.1.1"

nmcli con add type ethernet con-name br0-eth0 ifname eth0 master br0

nmcli con modify br0 bridge.stp no
nmcli con down "Wired connection 1"   # Desactivar perfil antiguo de eth0
nmcli con up br0
```

```bash
# 2. Definir el bridge en libvirt
cat > /tmp/br0-net.xml << 'EOF'
<network>
  <name>br0-network</name>
  <forward mode="bridge"/>
  <bridge name="br0"/>
</network>
EOF

virsh net-define /tmp/br0-net.xml
virsh net-start br0-network
virsh net-autostart br0-network
```

```bash
# 3. Crear VM usando el bridge
virt-install \
    --name mi-vm \
    --ram 2048 \
    --disk size=20 \
    --os-variant ubuntu22.04 \
    --network bridge=br0 \
    --cdrom ubuntu-22.04.iso
```

### Verificar que la VM está en el bridge

```bash
# Ver puertos del bridge (vnetX son las interfaces de las VMs)
bridge link show br0
```

```
2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> ... master br0
8: vnet0: <BROADCAST,MULTICAST,UP,LOWER_UP> ... master br0
9: vnet1: <BROADCAST,MULTICAST,UP,LOWER_UP> ... master br0
```

```bash
# Ver la tabla MAC del bridge
bridge fdb show br0 | head -20

# Verificar que la VM tiene IP en la red del bridge
virsh domifaddr mi-vm
```

---

## 9. Bridges en Docker

Docker usa bridges extensivamente. El bridge `docker0` es creado automáticamente al instalar Docker.

### Bridge por defecto (docker0)

```
┌──────────────────────────────────────────────┐
│                    Host                       │
│                                              │
│  docker0 (172.17.0.1/16)                     │
│  ├── veth1234 → Container-1 (172.17.0.2)    │
│  ├── veth5678 → Container-2 (172.17.0.3)    │
│  └── veth9abc → Container-3 (172.17.0.4)    │
│                                              │
│  eth0 (192.168.1.50) ← Red real             │
│       │                                      │
│       │ NAT (iptables)                       │
│       │ Contenedores salen a Internet        │
│       │ Port forwarding para acceso entrante │
└───────┼──────────────────────────────────────┘
        │
   Red física (192.168.1.0/24)
```

```bash
# Ver bridge docker0
ip link show docker0
bridge link show docker0

# Ver redes Docker
docker network ls
```

```
NETWORK ID     NAME      DRIVER    SCOPE
a1b2c3d4e5f6   bridge    bridge    local    ← docker0
f6e5d4c3b2a1   host      host      local
1234567890ab   none      null      local
```

### Crear bridge personalizado

```bash
# Red Docker personalizada (mejor que la default)
docker network create --driver bridge \
    --subnet 10.42.0.0/24 \
    --gateway 10.42.0.1 \
    --opt "com.docker.network.bridge.name=br-myapp" \
    myapp-net

# Ver el bridge que Docker creó
ip link show br-myapp
bridge link show br-myapp

# Usar la red
docker run -d --network myapp-net --name web nginx
docker run -d --network myapp-net --name api node:18

# Los contenedores en la misma red pueden comunicarse por nombre
docker exec web ping api
```

### Diferencias entre bridge default y personalizado

| Característica | Bridge default (docker0) | Bridge personalizado |
|----------------|--------------------------|---------------------|
| DNS entre contenedores | Solo por IP | Por nombre (DNS integrado) |
| Aislamiento | Todos los contenedores juntos | Solo contenedores en esta red |
| Configuración | Limitada | Subnet, gateway, opciones |
| Recomendación | Solo para testing rápido | **Producción** |

### Conectar contenedor a la red del host (sin NAT)

```bash
# Opción 1: host network (sin aislamiento de red)
docker run -d --network host nginx
# El contenedor comparte la red del host completamente

# Opción 2: macvlan (contenedor con IP en la red física)
docker network create -d macvlan \
    --subnet 192.168.1.0/24 \
    --gateway 192.168.1.1 \
    -o parent=eth0 \
    lan-net

docker run -d --network lan-net --ip 192.168.1.100 nginx
# El contenedor tiene IP real en la LAN
```

---

## 10. Bridge vs macvlan vs ipvlan

Cuando necesitas conectar VMs o contenedores a la red, hay tres opciones principales:

```
BRIDGE:
  Host                        Red física
  ┌───────────┐
  │ br0       │
  │ ├── eth0 ─┼──────────── Switch
  │ ├── VM-1  │              │
  │ └── VM-2  │              │
  └───────────┘
  Cada VM tiene su propia MAC
  Bridge aprende MACs, reenvía tramas
  El switch ve múltiples MACs en un puerto

MACVLAN:
  Host                        Red física
  ┌───────────┐
  │ eth0 ─────┼──────────── Switch
  │ ├── macv0 │(MAC propia)  │
  │ └── macv1 │(MAC propia)  │
  └───────────┘
  Sin bridge intermediario
  Cada interfaz macvlan tiene MAC única
  Más eficiente que bridge
  Host NO puede comunicarse con macvlan (sin bridge mode)

IPVLAN:
  Host                        Red física
  ┌───────────┐
  │ eth0 ─────┼──────────── Switch
  │ ├── ipv0  │(misma MAC)  │
  │ └── ipv1  │(misma MAC)  │
  └───────────┘
  Todos comparten la MAC de eth0
  Diferenciación por IP
  Útil cuando el switch limita MACs por puerto
```

| Criterio | Bridge | Macvlan | Ipvlan |
|----------|--------|---------|--------|
| Complejidad | Media | Baja | Baja |
| Rendimiento | Bueno | Mejor | Mejor |
| MACs adicionales | Sí (una por VM/container) | Sí | No (comparten) |
| Host ↔ VM/container | Sí | No (sin workaround) | Depende del modo |
| Limit MACs en switch | Puede ser problema | Puede ser problema | No (1 MAC) |
| DHCP | Funciona | Funciona | Problemático (misma MAC) |
| Caso típico | VMs, Docker default | Docker en LAN | Entornos con restricción de MAC |

### Cuándo usar cada uno

```
¿El host necesita comunicarse con los VMs/containers directamente?
├── SÍ → Bridge
└── NO → ¿El switch limita MACs por puerto?
          ├── SÍ → Ipvlan
          └── NO → Macvlan (más simple y eficiente que bridge)
```

---

## 11. Diagnóstico

### Ver bridges y sus puertos

```bash
# Con ip/bridge (moderno)
bridge link show
```

```
2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 master br0 state forwarding
8: vnet0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 master br0 state forwarding
9: vnet1: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 master br0 state forwarding
```

```bash
# Con brctl (legacy, requiere bridge-utils)
brctl show
```

```
bridge name     bridge id               STP enabled     interfaces
br0             8000.aabbcc112233       no              eth0
                                                        vnet0
                                                        vnet1
docker0         8000.0242a1b2c3d4       no              veth1234
```

### Tabla MAC (FDB)

```bash
# Ver tabla de reenvío
bridge fdb show br0
```

```
aa:bb:cc:11:22:33 dev eth0 master br0
dd:ee:ff:44:55:66 dev vnet0 master br0
11:22:33:44:55:66 dev vnet1 master br0
33:33:00:00:00:01 dev br0 self permanent
```

```bash
# Solo entradas aprendidas (no permanentes)
bridge fdb show br0 | grep -v permanent

# Buscar una MAC específica
bridge fdb show br0 | grep "dd:ee:ff"
```

### Estado STP de los puertos

```bash
# Con bridge command
bridge link show br0
# state: forwarding, blocking, listening, learning, disabled

# Detalles STP
brctl showstp br0
```

```
br0
 bridge id              8000.aabbcc112233
 designated root        8000.aabbcc112233
 root port                 0                    path cost          0
 max age                  20.00                 bridge max age    20.00
 hello time                2.00                 bridge hello time  2.00
 forward delay             4.00                 bridge forward delay 4.00
 ageing time             300.00

eth0 (1)
 port id                8001                    state            forwarding
 designated root        8000.aabbcc112233       path cost          4
 designated bridge      8000.aabbcc112233       message age timer  0.00
```

### Estadísticas

```bash
# Tráfico por puerto del bridge
ip -s link show eth0
ip -s link show vnet0

# Capturar tráfico en el bridge
sudo tcpdump -i br0 -nn

# Capturar tráfico en un puerto específico del bridge
sudo tcpdump -i vnet0 -nn
```

### Problemas comunes de diagnóstico

```bash
# ¿El bridge está UP?
ip link show br0 | grep -o "state [A-Z]*"

# ¿Tiene puertos?
bridge link show br0

# ¿Los puertos están en estado forwarding?
bridge link show br0 | grep state

# ¿La tabla MAC tiene entradas?
bridge fdb show br0 | grep -v permanent | wc -l

# ¿El bridge tiene IP?
ip addr show br0

# ¿Hay ruta por defecto vía el bridge?
ip route show default

# ¿iptables está interfiriendo?
# (Docker añade reglas que pueden afectar bridges)
sudo iptables -L FORWARD -n -v | head -20
```

---

## 12. Errores comunes

### Error 1: Perder conectividad SSH al agregar la interfaz al bridge

```
PROBLEMA:
# Conectado por SSH a 192.168.1.50 vía eth0
sudo ip link set eth0 master br0
# ¡Conexión SSH perdida!

# Al agregar eth0 al bridge, eth0 pierde su IP.
# El bridge (br0) no tiene IP todavía.

CORRECTO:
# Hacer todo en una sola línea o script:
sudo ip link add br0 type bridge && \
sudo ip link set br0 up && \
sudo ip addr del 192.168.1.50/24 dev eth0 && \
sudo ip link set eth0 master br0 && \
sudo ip addr add 192.168.1.50/24 dev br0 && \
sudo ip route add default via 192.168.1.1 dev br0

# O mejor: usar NetworkManager/systemd-networkd que hace
# la transición atómicamente

# O el método más seguro: programar un rollback
echo "ip link set eth0 nomaster; ip addr add 192.168.1.50/24 dev eth0; \
ip route add default via 192.168.1.1 dev eth0" | at now + 3 minutes
# Luego hacer el cambio. Si falla, se revierte en 3 minutos.
```

### Error 2: Forward delay causa retraso al conectar VMs

```
PROBLEMA:
# VM arranca y no tiene red durante 30 segundos
# STP está habilitado con forward_delay por defecto (15s × 2 fases)

CORRECTO:
# Si no hay riesgo de bucles (topología simple), deshabilitar STP:
sudo ip link set br0 type bridge stp_state 0

# O reducir forward delay si necesitas STP:
sudo ip link set br0 type bridge forward_delay 200
# 200 centésimas = 2 segundos por fase = 4s total

# En NetworkManager:
nmcli con modify br0 bridge.stp no bridge.forward-delay 0

# En systemd-networkd:
# [Bridge]
# STP=false
# ForwardDelaySec=0
```

### Error 3: Docker iptables interfiere con bridges manuales

```
PROBLEMA:
# Creaste un bridge br0 para VMs
# Docker está instalado y añade reglas iptables
# El tráfico entre puertos de br0 es filtrado por iptables

# Docker habilita: net.bridge.bridge-nf-call-iptables = 1
# Esto hace que el tráfico del bridge pase por iptables
# → reglas de Docker (DROP en FORWARD) bloquean tráfico legítimo

SOLUCIÓN:
# Opción 1: deshabilitar bridge-nf-call para tu bridge
echo 0 > /proc/sys/net/bridge/bridge-nf-call-iptables

# Opción 2 (persistente):
echo "net.bridge.bridge-nf-call-iptables = 0" >> /etc/sysctl.d/99-bridge.conf
sudo sysctl -p /etc/sysctl.d/99-bridge.conf

# Opción 3: permitir tráfico de tu bridge específicamente
sudo iptables -A FORWARD -i br0 -o br0 -j ACCEPT

# NOTA: deshabilitar bridge-nf-call puede afectar las reglas
# de Docker. Evalúa el impacto.
```

### Error 4: Crear bridge sin puertos (bridge vacío no hace nada útil)

```
PROBLEMA:
sudo ip link add br0 type bridge
sudo ip link set br0 up
sudo ip addr add 192.168.1.50/24 dev br0
# "El bridge tiene IP pero no puedo llegar a nada"

# Un bridge sin puertos es como un switch sin cables.
# El host puede usar la IP del bridge para comunicarse
# consigo mismo, pero no tiene salida a la red.

CORRECTO:
# Agregar al menos una interfaz física (o virtual)
sudo ip link set eth0 master br0
# Ahora br0 puede comunicarse vía eth0
```

### Error 5: Asignar IP a eth0 Y al bridge simultáneamente

```
INCORRECTO:
# eth0 tiene IP: 192.168.1.50/24
# eth0 es miembro de br0
# br0 también tiene IP: 192.168.1.51/24
# → Conflicto: ¿el tráfico para 192.168.1.0/24 va por eth0 o br0?

CORRECTO:
# Cuando eth0 es miembro de un bridge:
#   eth0 NO debe tener IP
#   Solo br0 tiene IP

# eth0 opera en capa 2 (reenvía tramas al bridge)
# br0 opera en capa 3 (tiene IP, rutas)
```

---

## 13. Cheatsheet

```
CREAR BRIDGE
──────────────────────────────────────────────
ip link:
  ip link add br0 type bridge
  ip link set br0 up
  ip link set eth0 master br0
  ip addr add IP/M dev br0

NetworkManager:
  nmcli con add type bridge con-name br0 ifname br0
  nmcli con add type ethernet con-name br0-eth0 \
      ifname eth0 master br0
  nmcli con modify br0 bridge.stp no
  nmcli con up br0

systemd-networkd:
  .netdev:  [NetDev] Name=br0  Kind=bridge
  slaves:   [Match] Name=eth0
            [Network] Bridge=br0
  config:   [Match] Name=br0
            [Network] Address=IP/M  Gateway=GW

ifupdown:
  bridge_ports eth0
  bridge_stp off
  bridge_fd 0

GESTIONAR PUERTOS
──────────────────────────────────────────────
ip link set eth0 master br0      Agregar puerto
ip link set eth0 nomaster        Quitar puerto
bridge link show br0             Ver puertos

DIAGNÓSTICO
──────────────────────────────────────────────
bridge link show                 Puertos y estado
bridge fdb show br0              Tabla MAC (FDB)
brctl show                       Bridges (legacy)
brctl showstp br0                Estado STP (legacy)
ip -d link show br0              Propiedades del bridge
tcpdump -i br0                   Capturar tráfico

STP
──────────────────────────────────────────────
ip link set br0 type bridge stp_state 1      Habilitar
ip link set br0 type bridge stp_state 0      Deshabilitar
ip link set br0 type bridge forward_delay N  Forward delay (cs)

DOCKER BRIDGES
──────────────────────────────────────────────
docker network ls                Listar redes
docker network create NAME       Crear red (bridge)
docker network inspect NAME      Ver detalles
ip link show docker0             Ver bridge default
bridge link show docker0         Ver contenedores

USO CON VMs
──────────────────────────────────────────────
virt-install --network bridge=br0
virsh domifaddr VM-NAME
bridge link show br0             Ver interfaces vnet*

BRIDGE vs MACVLAN vs IPVLAN
──────────────────────────────────────────────
Bridge:  switch virtual, múltiples MACs, host ↔ VM ✓
Macvlan: sin bridge, MACs propias, host ↔ VM ✗
Ipvlan:  sin bridge, MAC compartida, sin prob. MAC limit
```

---

## 14. Ejercicios

### Ejercicio 1: Explorar los bridges existentes

En tu sistema:

1. Ejecuta `bridge link show`. ¿Hay bridges configurados? Si usas Docker, deberías ver `docker0`.
2. Si hay un bridge, muestra su tabla FDB con `bridge fdb show`. ¿Cuántas entradas hay? ¿Cuáles son permanentes y cuáles aprendidas?
3. Ejecuta `ip -d link show docker0` (o tu bridge). Identifica: STP habilitado/deshabilitado, forward delay, ageing time.
4. Si tienes contenedores Docker corriendo, ejecuta `bridge link show docker0`. ¿Cuántas interfaces `veth*` hay? Cada una corresponde a un contenedor.
5. Compara la salida de `brctl show` con `bridge link show`. ¿Qué información adicional proporciona cada uno?

### Ejercicio 2: Crear un bridge para VMs

En una VM (para no afectar tu red real):

1. Anota la IP, gateway y DNS actuales de tu interfaz principal.
2. Crea un bridge `br0` con `ip link` (temporalmente).
3. Mueve tu interfaz principal al bridge (cuidado con la pérdida de conectividad si estás en SSH — ejecuta todo en una línea).
4. Asigna la IP original al bridge.
5. Verifica con `bridge link show` que la interfaz es miembro del bridge.
6. Verifica con `ping` que tienes conectividad a Internet.
7. Deshaz los cambios restaurando la configuración original.

### Ejercicio 3: Docker networking en detalle

Requiere Docker instalado:

1. Crea una red Docker personalizada llamada `test-net` con subred 10.42.0.0/24.
2. Identifica el bridge Linux que Docker creó (`ip link show type bridge`).
3. Lanza dos contenedores en esa red: `web` (nginx) y `client` (alpine con sleep).
4. Desde `client`, haz `ping web`. ¿Funciona la resolución DNS por nombre?
5. Examina `bridge fdb show <bridge>` mientras los contenedores están corriendo. ¿Ves las MACs de los contenedores?
6. Detén un contenedor y vuelve a mirar la tabla FDB. ¿La entrada desapareció inmediatamente o después del ageing time?
7. Elimina la red y verifica que el bridge desapareció.

**Pregunta de reflexión**: Un administrador tiene un hipervisor KVM con 20 VMs. Actualmente usa un bridge (`br0`) conectado a `eth0` para dar red a todas las VMs. Todas las VMs están en la misma subred 192.168.1.0/24. El equipo de seguridad pide aislar 5 VMs de producción del resto. ¿Cómo combinarías bridges y VLANs para lograr este aislamiento? ¿Necesitarías configuración en el switch físico? Dibuja la topología propuesta.

---

> **Siguiente tema**: T05 — VPN tunnels (WireGuard, comparación con OpenVPN)
