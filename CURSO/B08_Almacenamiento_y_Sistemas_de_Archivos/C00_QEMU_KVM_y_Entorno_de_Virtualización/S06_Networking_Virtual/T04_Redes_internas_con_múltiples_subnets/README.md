# Redes internas con múltiples subnets

## Índice

1. [El escenario: simular una red empresarial](#el-escenario-simular-una-red-empresarial)
2. [Diseño de topología](#diseño-de-topología)
3. [Crear las redes aisladas](#crear-las-redes-aisladas)
4. [Crear las VMs del escenario](#crear-las-vms-del-escenario)
5. [La VM router: dos interfaces, dos subnets](#la-vm-router-dos-interfaces-dos-subnets)
6. [Habilitar routing en la VM router](#habilitar-routing-en-la-vm-router)
7. [Configurar rutas en los clientes](#configurar-rutas-en-los-clientes)
8. [Verificar la comunicación entre subnets](#verificar-la-comunicación-entre-subnets)
9. [Topologías avanzadas](#topologías-avanzadas)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## El escenario: simular una red empresarial

En redes reales, los equipos están separados en **subnets** (subredes)
por función: oficina, servidores, DMZ, gestión. Un router conecta las
subnets y controla qué tráfico puede pasar entre ellas.

Con redes aisladas de libvirt podemos reproducir esto completamente dentro
del host, sin afectar ninguna red real:

```
┌──────────────────────────────────────────────────────────────────┐
│  HOST (todo virtual, todo aislado)                              │
│                                                                  │
│  Red "office"                    Red "servers"                   │
│  10.0.1.0/24                     10.0.2.0/24                    │
│                                                                  │
│  ┌────────┐  ┌────────┐         ┌────────┐  ┌────────┐         │
│  │ PC-1   │  │ PC-2   │         │ WEB    │  │ DB     │         │
│  │.1.10   │  │.1.11   │         │.2.10   │  │.2.11   │         │
│  └───┬────┘  └───┬────┘         └───┬────┘  └───┬────┘         │
│      │           │                  │           │               │
│  ────┴───────────┴──────       ─────┴───────────┴──────         │
│         virbr-off                      virbr-srv                │
│              │                              │                   │
│              │      ┌──────────────┐        │                   │
│              └──────┤   ROUTER     ├────────┘                   │
│                     │ .1.254 │.2.254│                            │
│                     │ ip_forward=1 │                            │
│                     └──────────────┘                            │
│                                                                  │
│  PC-1 (.1.10) → Router (.1.254 → .2.254) → WEB (.2.10)  ✅     │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

Este es el patrón que usaremos en B09 (Redes) para practicar routing, NAT
y firewall. En este tema preparamos la infraestructura.

---

## Diseño de topología

Antes de crear nada, diseñamos la topología completa en papel:

### Topología base del curso

```
Red "office"  (10.0.1.0/24)     Red "servers" (10.0.2.0/24)
────────────────────────         ─────────────────────────────
DHCP: 10.0.1.10 — .254          DHCP: 10.0.2.10 — .254
Gateway: 10.0.1.254 (router)    Gateway: 10.0.2.254 (router)

VMs:                             VMs:
  pc-1:  10.0.1.10               web:   10.0.2.10
  pc-2:  10.0.1.11               db:    10.0.2.11

                  VM Router:
                    enp1s0:  10.0.1.254  (office)
                    enp7s0:  10.0.2.254  (servers)
                    ip_forward = 1
```

### Tabla de direccionamiento

| VM | Red | IP | Gateway | Rol |
|----|-----|-------|---------|-----|
| router | office | 10.0.1.254 | — | Router entre subnets |
| router | servers | 10.0.2.254 | — | Router entre subnets |
| pc-1 | office | 10.0.1.10 | 10.0.1.254 | Cliente de oficina |
| pc-2 | office | 10.0.1.11 | 10.0.1.254 | Cliente de oficina |
| web | servers | 10.0.2.10 | 10.0.2.254 | Servidor web |
| db | servers | 10.0.2.11 | 10.0.2.254 | Servidor de BD |

> **Convención**: usamos `.254` para el router en cada subnet. Esto es una
> convención común en redes reales (gateway al final del rango).

---

## Crear las redes aisladas

Necesitamos dos redes aisladas, cada una con DHCP configurado para asignar
IPs en el rango correcto y anunciar el router como gateway.

### Red office

```bash
cat > /tmp/net-office.xml <<'EOF'
<network>
  <name>office</name>
  <bridge name='virbr-off' stp='on' delay='0'/>
  <ip address='10.0.1.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.0.1.10' end='10.0.1.200'/>
    </dhcp>
  </ip>
</network>
EOF
```

### Red servers

```bash
cat > /tmp/net-servers.xml <<'EOF'
<network>
  <name>servers</name>
  <bridge name='virbr-srv' stp='on' delay='0'/>
  <ip address='10.0.2.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.0.2.10' end='10.0.2.200'/>
    </dhcp>
  </ip>
</network>
EOF
```

### Definir y arrancar

```bash
sudo virsh net-define /tmp/net-office.xml
sudo virsh net-define /tmp/net-servers.xml
sudo virsh net-start office
sudo virsh net-start servers
sudo virsh net-autostart office
sudo virsh net-autostart servers
```

### Verificar

```bash
virsh net-list
```

```
 Name       State    Autostart   Persistent
----------------------------------------------
 default    active   yes         yes
 office     active   yes         yes
 servers    active   yes         yes
```

```bash
ip addr show virbr-off
# inet 10.0.1.1/24

ip addr show virbr-srv
# inet 10.0.2.1/24
```

> **Nota sobre el host**: el host tiene IP `10.0.1.1` en la red office y
> `10.0.2.1` en la red servers (las IPs de los bridges). Pero el host
> **no** es el router para las VMs — la VM router lo será. Las IPs del
> host en los bridges se usan para que libvirt pueda ejecutar dnsmasq.

---

## Crear las VMs del escenario

### VM Router (con dos interfaces)

```bash
# Overlay del sistema
sudo qemu-img create -f qcow2 \
    -b /var/lib/libvirt/images/tpl-debian12.qcow2 -F qcow2 \
    /var/lib/libvirt/images/router.qcow2

# Crear VM con una interfaz en cada red + default para internet
sudo virt-install \
    --name router \
    --ram 512 --vcpus 1 --import \
    --disk path=/var/lib/libvirt/images/router.qcow2 \
    --os-variant debian12 \
    --network network=default \
    --network network=office \
    --network network=servers \
    --graphics none --noautoconsole
```

El router tiene **tres** interfaces:
- `enp1s0`: red default (para que el router tenga internet y pueda instalar
  paquetes)
- `enp7s0`: red office (10.0.1.0/24)
- `enp8s0`: red servers (10.0.2.0/24)

> **Nota**: los nombres de interfaz (`enp1s0`, `enp7s0`, `enp8s0`) pueden
> variar según la distribución y la versión de systemd. Dentro de la VM,
> usa `ip link` para ver los nombres reales.

### VMs de oficina

```bash
for vm in pc-1 pc-2; do
    sudo qemu-img create -f qcow2 \
        -b /var/lib/libvirt/images/tpl-debian12.qcow2 -F qcow2 \
        /var/lib/libvirt/images/${vm}.qcow2
    sudo virt-install \
        --name $vm \
        --ram 512 --vcpus 1 --import \
        --disk path=/var/lib/libvirt/images/${vm}.qcow2 \
        --os-variant debian12 \
        --network network=default \
        --network network=office \
        --graphics none --noautoconsole
done
```

### VMs de servidores

```bash
for vm in web db; do
    sudo qemu-img create -f qcow2 \
        -b /var/lib/libvirt/images/tpl-debian12.qcow2 -F qcow2 \
        /var/lib/libvirt/images/${vm}.qcow2
    sudo virt-install \
        --name $vm \
        --ram 512 --vcpus 1 --import \
        --disk path=/var/lib/libvirt/images/${vm}.qcow2 \
        --os-variant debian12 \
        --network network=default \
        --network network=servers \
        --graphics none --noautoconsole
done
```

### Verificar todas las VMs

```bash
virsh list
```

```
 Id   Name     State
-----------------------
 1    router   running
 2    pc-1     running
 3    pc-2     running
 4    web      running
 5    db       running
```

---

## La VM router: dos interfaces, dos subnets

Dentro de la VM router, debemos configurar IPs estáticas en las interfaces
de las redes aisladas.

### Identificar las interfaces

```bash
virsh console router
# Login: labuser / labpass123

ip link
```

```
1: lo: <LOOPBACK,UP> ...
2: enp1s0: <BROADCAST,MULTICAST,UP> ...    ← default (DHCP: 192.168.122.x)
3: enp7s0: <BROADCAST,MULTICAST,UP> ...    ← office  (DHCP: 10.0.1.x)
4: enp8s0: <BROADCAST,MULTICAST,UP> ...    ← servers (DHCP: 10.0.2.x)
```

### Asignar IPs fijas al router

El DHCP de las redes aisladas asigna IPs automáticamente, pero el router
necesita IPs predecibles. Configuramos IPs estáticas:

```bash
# Dentro del router — asignar IP fija en la red office
sudo ip addr flush dev enp7s0
sudo ip addr add 10.0.1.254/24 dev enp7s0
sudo ip link set enp7s0 up

# Asignar IP fija en la red servers
sudo ip addr flush dev enp8s0
sudo ip addr add 10.0.2.254/24 dev enp8s0
sudo ip link set enp8s0 up
```

### Verificar

```bash
ip addr show enp7s0
# inet 10.0.1.254/24

ip addr show enp8s0
# inet 10.0.2.254/24
```

### Hacer la configuración persistente

Para que sobreviva reinicios, configura con NetworkManager o
`/etc/network/interfaces`:

**Con NetworkManager (AlmaLinux/Fedora):**

```bash
sudo nmcli connection modify "Wired connection 2" \
    ipv4.addresses 10.0.1.254/24 \
    ipv4.method manual

sudo nmcli connection modify "Wired connection 3" \
    ipv4.addresses 10.0.2.254/24 \
    ipv4.method manual

sudo nmcli connection up "Wired connection 2"
sudo nmcli connection up "Wired connection 3"
```

**Con /etc/network/interfaces (Debian):**

```bash
sudo tee -a /etc/network/interfaces <<'EOF'

auto enp7s0
iface enp7s0 inet static
    address 10.0.1.254
    netmask 255.255.255.0

auto enp8s0
iface enp8s0 inet static
    address 10.0.2.254
    netmask 255.255.255.0
EOF

sudo systemctl restart networking
```

---

## Habilitar routing en la VM router

Por defecto, Linux **no** reenvía paquetes entre interfaces. Necesitamos
habilitar IP forwarding para que el router pase tráfico de office a
servers y viceversa.

### Habilitar temporalmente

```bash
sudo sysctl -w net.ipv4.ip_forward=1
```

### Habilitar permanentemente

```bash
echo "net.ipv4.ip_forward = 1" | sudo tee /etc/sysctl.d/99-router.conf
sudo sysctl -p /etc/sysctl.d/99-router.conf
```

### Verificar

```bash
sysctl net.ipv4.ip_forward
# net.ipv4.ip_forward = 1

cat /proc/sys/net/ipv4/ip_forward
# 1
```

### Qué hace ip_forward

```
Sin ip_forward (valor 0):

  pc-1 (10.0.1.10) envía paquete a web (10.0.2.10)
       │
       ▼
  Router recibe en enp7s0 (10.0.1.254)
       │
       ▼
  Destino 10.0.2.10 está en enp8s0 → DESCARTAR
  (el kernel no reenvía entre interfaces)


Con ip_forward (valor 1):

  pc-1 (10.0.1.10) envía paquete a web (10.0.2.10)
       │
       ▼
  Router recibe en enp7s0 (10.0.1.254)
       │
       ▼
  Destino 10.0.2.10 está en enp8s0 → REENVIAR
  (el kernel actúa como router)
       │
       ▼
  web (10.0.2.10) recibe el paquete
```

---

## Configurar rutas en los clientes

Las VMs cliente necesitan saber que para alcanzar la otra subnet deben
pasar por el router.

### En pc-1 (red office, quiere alcanzar servers)

```bash
virsh console pc-1
# Añadir ruta: para 10.0.2.0/24, usar router 10.0.1.254
sudo ip route add 10.0.2.0/24 via 10.0.1.254
```

### En pc-2

```bash
virsh console pc-2
sudo ip route add 10.0.2.0/24 via 10.0.1.254
```

### En web (red servers, quiere alcanzar office)

```bash
virsh console web
sudo ip route add 10.0.1.0/24 via 10.0.2.254
```

### En db

```bash
virsh console db
sudo ip route add 10.0.1.0/24 via 10.0.2.254
```

### Verificar las rutas

```bash
# Dentro de pc-1:
ip route
```

```
default via 192.168.122.1 dev enp1s0          ← internet via default
10.0.1.0/24 dev enp7s0 proto kernel            ← red office (local)
10.0.2.0/24 via 10.0.1.254 dev enp7s0         ← red servers (via router)
192.168.122.0/24 dev enp1s0 proto kernel       ← red default (local)
```

### Alternativa: DHCP con opción gateway

En vez de añadir rutas manualmente en cada cliente, puedes configurar
las redes aisladas para que el DHCP anuncie el router como gateway.
Pero esto solo funciona como **default gateway**, no como ruta hacia
una subnet específica. Para labs simples con solo dos subnets, puedes
modificar el XML de la red:

```xml
<ip address='10.0.1.1' netmask='255.255.255.0'>
  <dhcp>
    <range start='10.0.1.10' end='10.0.1.200'/>
    <!-- No añadir gateway aquí — el host (10.0.1.1) no es router -->
  </dhcp>
</ip>
```

> **Predicción**: si pones el router (10.0.1.254) como gateway en el DHCP,
> los clientes lo usarán como **default gateway** para TODO el tráfico,
> incluido internet. Esto podría romper su conectividad si el router no
> tiene NAT configurado hacia la red default.

---

## Verificar la comunicación entre subnets

### Test completo paso a paso

```bash
# 1. Desde pc-1, ping al router (mismo subnet)
virsh console pc-1
ping -c 2 10.0.1.254
# ✅ Router accesible en la red office

# 2. Desde pc-1, ping al servidor web (otra subnet, via router)
ping -c 2 10.0.2.10
# ✅ Si el routing funciona

# 3. Desde web, ping a pc-1 (dirección inversa)
virsh console web
ping -c 2 10.0.1.10
# ✅ Routing bidireccional

# 4. Traceroute para ver el camino
traceroute 10.0.2.10
# 1  10.0.1.254 (router)
# 2  10.0.2.10 (web)
```

### Diagrama del flujo de un paquete

```
pc-1 (10.0.1.10) quiere contactar web (10.0.2.10):

  pc-1: ip route → 10.0.2.0/24 via 10.0.1.254
       │
       ▼
  Paquete: src=10.0.1.10 dst=10.0.2.10
  Enviado a 10.0.1.254 (router, MAC del router)
       │
  ─────┴───── virbr-off ─────────
                │
       Router recibe en enp7s0
       ip_forward=1 → reenviar
       Consulta tabla de rutas: 10.0.2.0/24 → enp8s0 (directamente conectado)
                │
  ─────────── virbr-srv ─────┴───
                                │
                          web recibe
                          Respuesta: src=10.0.2.10 dst=10.0.1.10
                          ip route → 10.0.1.0/24 via 10.0.2.254
                                │
                          (camino inverso por el router)
```

---

## Topologías avanzadas

### Tres subnets: oficina + servidores + DMZ

```bash
# Añadir una tercera red para la DMZ
cat > /tmp/net-dmz.xml <<'EOF'
<network>
  <name>dmz</name>
  <bridge name='virbr-dmz' stp='on' delay='0'/>
  <ip address='10.0.3.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.0.3.10' end='10.0.3.200'/>
    </dhcp>
  </ip>
</network>
EOF

sudo virsh net-define /tmp/net-dmz.xml
sudo virsh net-start dmz
```

```
  office          servers          dmz
  10.0.1.0/24     10.0.2.0/24     10.0.3.0/24

  PC-1 ───┐                    ┌─── proxy
  PC-2 ───┤                    │
           │    ┌──────────┐   │
           └────┤  Router  ├───┘
                │ .1.254   │
                │ .2.254   ├────── WEB
                │ .3.254   │       DB
                └──────────┘
```

El router necesita una interfaz adicional:

```bash
sudo virsh attach-interface router network dmz --model virtio --persistent
```

### Dos routers: simular ISP

```
  Red "lan"              Red "wan"              Red "isp"
  10.0.1.0/24           172.16.0.0/24           203.0.113.0/24

  PC-1 ─── Router-A ─── (enlace WAN) ─── Router-B ─── Server
           .1.254  │                     │ .113.254
                   │ .0.1       .0.2     │
                   └─────────────────────┘
```

Esto permite practicar routing multi-hop, BGP simplificado, o NAT de
borde.

### Script para crear topología completa

```bash
#!/bin/bash
# create-topology.sh — Crear topología de 2 subnets con router
set -euo pipefail

TEMPLATE="/var/lib/libvirt/images/tpl-debian12.qcow2"
IMGDIR="/var/lib/libvirt/images"

echo "=== Creando redes ==="
for net in office servers; do
    if ! virsh net-info $net &>/dev/null; then
        virsh net-define /tmp/net-${net}.xml
        virsh net-start $net
        virsh net-autostart $net
    fi
done

echo "=== Creando router ==="
qemu-img create -f qcow2 -b "$TEMPLATE" -F qcow2 "$IMGDIR/router.qcow2"
virt-install --name router --ram 512 --vcpus 1 --import \
    --disk path="$IMGDIR/router.qcow2" \
    --os-variant debian12 \
    --network network=default \
    --network network=office \
    --network network=servers \
    --graphics none --noautoconsole

echo "=== Creando PCs de oficina ==="
for vm in pc-1 pc-2; do
    qemu-img create -f qcow2 -b "$TEMPLATE" -F qcow2 "$IMGDIR/${vm}.qcow2"
    virt-install --name $vm --ram 512 --vcpus 1 --import \
        --disk path="$IMGDIR/${vm}.qcow2" \
        --os-variant debian12 \
        --network network=default \
        --network network=office \
        --graphics none --noautoconsole
done

echo "=== Creando servidores ==="
for vm in web db; do
    qemu-img create -f qcow2 -b "$TEMPLATE" -F qcow2 "$IMGDIR/${vm}.qcow2"
    virt-install --name $vm --ram 512 --vcpus 1 --import \
        --disk path="$IMGDIR/${vm}.qcow2" \
        --os-variant debian12 \
        --network network=default \
        --network network=servers \
        --graphics none --noautoconsole
done

echo ""
echo "=== Topología creada ==="
virsh list
echo ""
echo "Siguiente paso: configurar IPs y routing dentro de las VMs"
```

### Script de limpieza

```bash
#!/bin/bash
# destroy-topology.sh — Destruir toda la topología
set -euo pipefail

for vm in router pc-1 pc-2 web db; do
    virsh destroy $vm 2>/dev/null || true
    virsh undefine $vm --remove-all-storage 2>/dev/null || true
done

for net in office servers; do
    virsh net-destroy $net 2>/dev/null || true
    virsh net-undefine $net 2>/dev/null || true
done

echo "Topología eliminada."
```

---

## Errores comunes

### 1. Olvidar ip_forward en el router

```bash
# ❌ pc-1 puede hacer ping al router pero no a web
ping 10.0.1.254   # ✅
ping 10.0.2.10    # ❌ timeout

# Diagnóstico (en el router):
sysctl net.ipv4.ip_forward
# 0  ← deshabilitado

# ✅ Habilitar
sudo sysctl -w net.ipv4.ip_forward=1
```

### 2. Faltan rutas en los clientes

```bash
# ❌ pc-1 no sabe cómo llegar a 10.0.2.0/24
ping 10.0.2.10
# Network is unreachable

ip route
# (no hay ruta hacia 10.0.2.0/24)

# ✅ Añadir ruta
sudo ip route add 10.0.2.0/24 via 10.0.1.254
```

### 3. Ruta de ida pero no de vuelta

```bash
# ❌ pc-1 puede enviar paquete a web, pero web no sabe responder
# pc-1 tiene ruta a 10.0.2.0/24 via router ✅
# PERO web NO tiene ruta a 10.0.1.0/24 via router ❌

# Resultado: ping desde pc-1 a web → timeout
# El paquete llega a web, pero la respuesta se pierde

# ✅ Ambos lados necesitan rutas
# En pc-1: route add 10.0.2.0/24 via 10.0.1.254
# En web:  route add 10.0.1.0/24 via 10.0.2.254
```

El routing es **bidireccional**: el paquete debe poder ir Y volver. Si
solo configuras la ruta en un lado, el paquete llega pero la respuesta
se pierde.

### 4. Confundir IPs del host (bridge) con IPs del router

```bash
# El host tiene:
#   10.0.1.1 en virbr-off (para dnsmasq)
#   10.0.2.1 en virbr-srv (para dnsmasq)
#
# El router VM tiene:
#   10.0.1.254 en enp7s0
#   10.0.2.254 en enp8s0
#
# ❌ Configurar el gateway de los clientes como 10.0.1.1 (host)
# El host NO enruta entre redes aisladas
#
# ✅ El gateway debe ser 10.0.1.254 (la VM router)
```

### 5. Interfaces del router con nombres inesperados

```bash
# ❌ Asumiste enp1s0/enp7s0/enp8s0 pero los nombres son diferentes
ip link
# 2: ens3:    ← default
# 3: ens8:    ← office
# 4: ens9:    ← servers

# ✅ Siempre verificar con ip link antes de configurar
# Los nombres dependen del bus PCI virtual asignado por QEMU
```

---

## Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║  REDES INTERNAS CON MÚLTIPLES SUBNETS — REFERENCIA RÁPIDA          ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  DISEÑO:                                                           ║
║    1. Definir subnets (office: 10.0.1.0/24, servers: 10.0.2.0/24) ║
║    2. Asignar IPs al router (.254 en cada subnet)                  ║
║    3. Crear tabla de direccionamiento                              ║
║                                                                    ║
║  CREAR REDES AISLADAS:                                             ║
║    virsh net-define net-office.xml                                  ║
║    virsh net-define net-servers.xml                                 ║
║    virsh net-start office && virsh net-autostart office             ║
║    virsh net-start servers && virsh net-autostart servers           ║
║                                                                    ║
║  CREAR VM ROUTER (interfaz en cada red):                           ║
║    virt-install ... \                                              ║
║      --network network=default \                                   ║
║      --network network=office \                                    ║
║      --network network=servers                                     ║
║                                                                    ║
║  CONFIGURAR ROUTER (dentro de la VM):                              ║
║    ip addr add 10.0.1.254/24 dev enp7s0                            ║
║    ip addr add 10.0.2.254/24 dev enp8s0                            ║
║    sysctl -w net.ipv4.ip_forward=1                                 ║
║                                                                    ║
║  CONFIGURAR CLIENTES (dentro de cada VM):                          ║
║    Office → servers: ip route add 10.0.2.0/24 via 10.0.1.254      ║
║    Servers → office: ip route add 10.0.1.0/24 via 10.0.2.254      ║
║                                                                    ║
║  VERIFICAR:                                                        ║
║    ping DEST                    conectividad básica                ║
║    traceroute DEST              verificar camino via router        ║
║    ip route                     tabla de rutas                     ║
║    sysctl net.ipv4.ip_forward   forwarding habilitado              ║
║                                                                    ║
║  ERRORES FRECUENTES:                                               ║
║    • ip_forward=0 en el router → tráfico no cruza subnets         ║
║    • Ruta de ida sin ruta de vuelta → timeout                      ║
║    • Gateway 10.0.1.1 (host) en vez de 10.0.1.254 (router VM)    ║
║    • Nombres de interfaz asumidos sin verificar (ip link)          ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Construir la topología de dos subnets

**Objetivo**: crear la infraestructura completa de redes y VMs.

1. Crea los XMLs de las redes office y servers (como se muestra en la
   sección "Crear las redes aisladas").

2. Defínelas y arráncalas:
   ```bash
   sudo virsh net-define /tmp/net-office.xml
   sudo virsh net-define /tmp/net-servers.xml
   sudo virsh net-start office
   sudo virsh net-start servers
   ```

3. Crea al menos tres VMs: router, pc-1, web.
   El router con interfaces en default + office + servers.
   pc-1 con interfaces en default + office.
   web con interfaces en default + servers.

4. Espera a que arranquen y verifica:
   ```bash
   virsh list
   virsh net-dhcp-leases office
   virsh net-dhcp-leases servers
   ```

5. Dentro de cada VM, identifica las interfaces con `ip link` y
   `ip addr`. Anota qué interfaz está en qué red.

6. Dibuja la topología real basándote en las IPs asignadas. ¿Coincide
   con el diseño?

**Pregunta de reflexión**: ¿por qué damos a todas las VMs una interfaz
en la red default además de la red aislada? ¿Qué pasaría si el router
no tuviera acceso a internet?

---

### Ejercicio 2: Configurar routing y verificar

**Objetivo**: habilitar la comunicación entre las dos subnets.

1. Configura el router (IPs estáticas + ip_forward):
   ```bash
   virsh console router

   # Identificar interfaces
   ip link

   # Asignar IPs al router (adaptar nombres de interfaz)
   sudo ip addr flush dev enp7s0    # o el nombre real
   sudo ip addr add 10.0.1.254/24 dev enp7s0
   sudo ip link set enp7s0 up

   sudo ip addr flush dev enp8s0    # o el nombre real
   sudo ip addr add 10.0.2.254/24 dev enp8s0
   sudo ip link set enp8s0 up

   # Habilitar forwarding
   sudo sysctl -w net.ipv4.ip_forward=1

   # Verificar
   ip addr show
   sysctl net.ipv4.ip_forward
   ```

2. Configura pc-1:
   ```bash
   virsh console pc-1
   sudo ip route add 10.0.2.0/24 via 10.0.1.254
   ip route
   ```

3. Configura web:
   ```bash
   virsh console web
   sudo ip route add 10.0.1.0/24 via 10.0.2.254
   ip route
   ```

4. Prueba la comunicación:
   ```bash
   # Desde pc-1:
   ping -c 3 10.0.1.254    # router (misma subnet)
   ping -c 3 10.0.2.10     # web (otra subnet, via router)
   traceroute 10.0.2.10    # ¿pasa por 10.0.1.254?
   ```

5. Prueba en dirección inversa:
   ```bash
   # Desde web:
   ping -c 3 10.0.2.254    # router (misma subnet)
   ping -c 3 10.0.1.10     # pc-1 (otra subnet, via router)
   ```

6. Si algo falla, diagnostica:
   - ¿`ip_forward=1` en el router?
   - ¿Rutas correctas en ambos lados?
   - ¿Las IPs del router son correctas?
   - ¿Las interfaces están UP?

**Pregunta de reflexión**: ahora pc-1 puede alcanzar web pasando por el
router. Si quisieras que pc-1 pudiera alcanzar web pero que web **no**
pudiera iniciar conexiones hacia pc-1, ¿qué necesitarías configurar en
el router? (Pista: este es un lab de firewall de B11.)

---

### Ejercicio 3: Probar el aislamiento

**Objetivo**: demostrar que el router es el único camino entre subnets.

1. Con el routing funcionando, verifica que pc-1 alcanza web:
   ```bash
   # Desde pc-1:
   ping -c 2 10.0.2.10
   # ✅
   ```

2. Ahora deshabilita ip_forward en el router:
   ```bash
   virsh console router
   sudo sysctl -w net.ipv4.ip_forward=0
   ```

3. Intenta de nuevo desde pc-1:
   ```bash
   virsh console pc-1
   ping -c 3 10.0.2.10
   # ❌ timeout (el router ya no reenvía)
   ```

4. Rehabilita:
   ```bash
   virsh console router
   sudo sysctl -w net.ipv4.ip_forward=1
   ```

5. Verifica que vuelve a funcionar:
   ```bash
   virsh console pc-1
   ping -c 2 10.0.2.10
   # ✅ de vuelta
   ```

6. Elimina la ruta en pc-1 y verifica:
   ```bash
   sudo ip route del 10.0.2.0/24
   ping -c 2 10.0.2.10
   # ❌ "Network is unreachable" (no sabe cómo llegar)
   ```

7. Restaura:
   ```bash
   sudo ip route add 10.0.2.0/24 via 10.0.1.254
   ```

8. Limpia todo al terminar:
   ```bash
   for vm in router pc-1 pc-2 web db; do
       sudo virsh destroy $vm 2>/dev/null
       sudo virsh undefine $vm --remove-all-storage 2>/dev/null
   done
   sudo virsh net-destroy office && sudo virsh net-undefine office
   sudo virsh net-destroy servers && sudo virsh net-undefine servers
   ```

**Pregunta de reflexión**: con `ip_forward=0`, el router descarta los
paquetes silenciosamente. ¿Cómo podrías configurar el router para que
en vez de descartarlos, envíe un mensaje ICMP "destination unreachable"
al remitente? (Pista: esto es el comportamiento por defecto de iptables
con `REJECT` en vez de `DROP`.)
