# Recetas por bloque

## Índice

1. [Visión general](#1-visión-general)
2. [B08 — Almacenamiento](#2-b08--almacenamiento)
3. [B09 — Redes](#3-b09--redes)
4. [B10 — Servicios](#4-b10--servicios)
5. [B11 — Seguridad, Kernel y Boot](#5-b11--seguridad-kernel-y-boot)
6. [Organización de scripts](#6-organización-de-scripts)
7. [Errores comunes](#7-errores-comunes)
8. [Cheatsheet](#8-cheatsheet)
9. [Ejercicios](#9-ejercicios)

---

## 1. Visión general

Cada bloque del curso necesita un entorno de lab distinto. Esta referencia
reúne las recetas completas — scripts listos para levantar y destruir cada
entorno.

```
┌──────────────────────────────────────────────────────────────────┐
│                    ENTORNOS POR BLOQUE                          │
│                                                                  │
│  B08 Almacenamiento    B09 Redes         B10 Servicios          │
│  ┌──────┐ ┌──────┐     ┌──────┐          ┌──────┐ ┌──────┐     │
│  │Debian│ │Alma  │     │Router│          │ DNS  │ │Client│     │
│  │4 disk│ │4 disk│     │      │          │ DHCP │ │      │     │
│  └──┬───┘ └──┬───┘     └──┬───┘          └──┬───┘ └──┬───┘     │
│     │        │            │                 │        │          │
│  ┌──▼────────▼──┐    ┌──▼──┐ ┌──▼──┐    ┌──▼────────▼──┐      │
│  │   default    │    │offic│ │serv │    │   isolated   │      │
│  │    (NAT)     │    │ isolat│ │isolat│    │   + default  │      │
│  └──────────────┘    └─────┘ └─────┘    └──────────────┘      │
│                                                                  │
│  B11 Seguridad                                                   │
│  ┌──────┐ ┌──────┐                                               │
│  │Alma  │ │Debian│     Cada entorno: ~30s para levantar          │
│  │SELin.│ │AppAr.│     Un comando para destruir                  │
│  │1 disk│ │      │     Imágenes base nunca se tocan               │
│  └──┬───┘ └──┬───┘                                               │
│     │        │                                                   │
│  ┌──▼────────▼──┐                                                │
│  │   default    │                                                │
│  └──────────────┘                                                │
└──────────────────────────────────────────────────────────────────┘
```

### Prerequisitos comunes

Todos los scripts asumen:

```bash
# Imágenes base selladas (read-only, creadas en S04/T04)
ls -la /var/lib/libvirt/images/base-debian12.qcow2   # -r--r--r--
ls -la /var/lib/libvirt/images/base-alma9.qcow2      # -r--r--r--

# Librería de funciones (creada en T01)
ls ~/lab-scripts/lab-functions.sh

# Red default activa
virsh net-info default
```

---

## 2. B08 — Almacenamiento

### 2.1. Qué necesita

| Componente | Detalle |
|------------|---------|
| VMs | 1 Debian 12 + 1 AlmaLinux 9 |
| Discos extra | 4 × 2G por VM (particionado, LVM, RAID) |
| Redes | default (NAT) — para instalar paquetes |
| Redes extra | Ninguna |
| Snapshots | Opcional — antes de labs destructivos |

### 2.2. Diagrama

```
┌─────────────────────────────────────────────────────────────┐
│                   ENTORNO B08                               │
│                                                             │
│  ┌──────────────────────┐    ┌──────────────────────┐       │
│  │  lab-b08-debian      │    │  lab-b08-alma        │       │
│  │  Debian 12           │    │  AlmaLinux 9         │       │
│  │  2GB RAM / 2 vCPUs   │    │  2GB RAM / 2 vCPUs   │       │
│  │                      │    │                      │       │
│  │  vda: SO             │    │  vda: SO             │       │
│  │  vdb: 2G (part/raid) │    │  vdb: 2G             │       │
│  │  vdc: 2G (lvm/raid)  │    │  vdc: 2G             │       │
│  │  vdd: 2G (lvm/raid)  │    │  vdd: 2G             │       │
│  │  vde: 2G (spare)     │    │  vde: 2G             │       │
│  └────────┬─────────────┘    └────────┬─────────────┘       │
│           └──────────┬───────────────┘                      │
│                      │                                      │
│              ┌───────▼────────┐                             │
│              │    default     │                             │
│              │ 192.168.122.0  │                             │
│              │     (NAT)      │                             │
│              └────────────────┘                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.3. Script de creación

```bash
#!/bin/bash
# create-lab-b08.sh — B08 Storage lab environment
set -euo pipefail
source "$(dirname "$0")/lab-functions.sh"

PREFIX="lab-b08"
BASE_DEB="${POOL}/base-debian12.qcow2"
BASE_ALMA="${POOL}/base-alma9.qcow2"

echo "=== Creating B08 lab (Storage) ==="

# Debian VM
create_overlay "$BASE_DEB" "${POOL}/${PREFIX}-debian.qcow2"
create_vm "${PREFIX}-debian" "${POOL}/${PREFIX}-debian.qcow2" 2048 2 debian12
attach_lab_disks "${PREFIX}-debian" 4 2G
echo "  lab-b08-debian: created with 4 extra disks"

# AlmaLinux VM
create_overlay "$BASE_ALMA" "${POOL}/${PREFIX}-alma.qcow2"
create_vm "${PREFIX}-alma" "${POOL}/${PREFIX}-alma.qcow2" 2048 2 almalinux9
attach_lab_disks "${PREFIX}-alma" 4 2G
echo "  lab-b08-alma: created with 4 extra disks"

echo ""
echo "=== B08 lab ready ==="
echo "VMs:"
virsh list --all | grep "$PREFIX"
echo ""
echo "Access:"
echo "  virsh console ${PREFIX}-debian"
echo "  virsh console ${PREFIX}-alma"
```

### 2.4. Script de destrucción

```bash
#!/bin/bash
# destroy-lab-b08.sh — Teardown B08 lab
set -euo pipefail
source "$(dirname "$0")/lab-functions.sh"

PREFIX="lab-b08"

echo "=== Destroying B08 lab ==="

destroy_vm "${PREFIX}-debian"
cleanup_disks "${PREFIX}-debian" 4

destroy_vm "${PREFIX}-alma"
cleanup_disks "${PREFIX}-alma" 4

echo "=== B08 lab destroyed ==="
```

### 2.5. Uso típico durante B08

```bash
# Inicio del bloque: levantar el entorno
sudo ./create-lab-b08.sh

# Lab de particionado: usar vdb
#   (dentro de VM: fdisk /dev/vdb, mkfs, mount)

# Limpiar solo los discos para otro lab (sin destruir VMs):
# Opción A: recrear discos individuales (VM apagada)
sudo virsh shutdown lab-b08-debian
sudo qemu-img create -f qcow2 /var/lib/libvirt/images/lab-b08-debian-disk1.qcow2 2G
sudo virsh start lab-b08-debian

# Opción B: snapshot antes de cada lab
sudo virsh snapshot-create-as lab-b08-debian "pre-raid-lab"
#   ... hacer el lab ...
sudo virsh snapshot-revert lab-b08-debian "pre-raid-lab" --running

# Fin del bloque: destruir todo
sudo ./destroy-lab-b08.sh
```

---

## 3. B09 — Redes

### 3.1. Qué necesita

| Componente | Detalle |
|------------|---------|
| VMs | 3: router + host-office + host-servers |
| Discos extra | Ninguno |
| Redes | default (NAT para el router) + 2 redes aisladas |
| Propósito | Routing, NAT, firewall entre subnets |

### 3.2. Diagrama

```
┌──────────────────────────────────────────────────────────────────┐
│                       ENTORNO B09                               │
│                                                                  │
│           ┌───────────────────────────────┐                      │
│           │       lab-b09-router          │                      │
│           │       Debian 12               │                      │
│           │       2GB RAM / 2 vCPUs       │                      │
│           │                               │                      │
│           │  eth0: default (NAT, internet)│                      │
│           │  eth1: 10.0.1.1 (office)      │                      │
│           │  eth2: 10.0.2.1 (servers)     │                      │
│           └───┬──────────┬───────────┬────┘                      │
│               │          │           │                           │
│         ┌─────▼───┐  ┌───▼────┐  ┌───▼─────┐                    │
│         │ default │  │ office │  │ servers │                    │
│         │  (NAT)  │  │10.0.1.0│  │10.0.2.0 │                    │
│         └─────────┘  │  /24   │  │  /24    │                    │
│                      └───┬────┘  └───┬─────┘                    │
│                          │           │                           │
│              ┌───────────▼──┐  ┌─────▼──────────┐               │
│              │lab-b09-office│  │lab-b09-servers  │               │
│              │  Debian 12   │  │  AlmaLinux 9    │               │
│              │  1GB / 1 vCPU│  │  1GB / 1 vCPU   │               │
│              │              │  │                  │               │
│              │  eth0:       │  │  eth0:           │               │
│              │  10.0.1.10   │  │  10.0.2.10       │               │
│              │              │  │                  │               │
│              │  gw: 10.0.1.1│  │  gw: 10.0.2.1   │               │
│              └──────────────┘  └──────────────────┘               │
└──────────────────────────────────────────────────────────────────┘
```

### 3.3. Script de creación

```bash
#!/bin/bash
# create-lab-b09.sh — B09 Networking lab environment
set -euo pipefail
source "$(dirname "$0")/lab-functions.sh"

PREFIX="lab-b09"
BASE_DEB="${POOL}/base-debian12.qcow2"
BASE_ALMA="${POOL}/base-alma9.qcow2"

echo "=== Creating B09 lab (Networking) ==="

# ── Create isolated networks ────────────────────────────────────
echo "Creating isolated networks..."

cat > /tmp/net-office.xml << 'EOF'
<network>
  <name>lab-b09-office</name>
  <bridge name="virbr-office"/>
  <ip address="10.0.1.1" netmask="255.255.255.0">
    <dhcp>
      <range start="10.0.1.10" end="10.0.1.50"/>
    </dhcp>
  </ip>
</network>
EOF

cat > /tmp/net-servers.xml << 'EOF'
<network>
  <name>lab-b09-servers</name>
  <bridge name="virbr-server"/>
  <ip address="10.0.2.1" netmask="255.255.255.0">
    <dhcp>
      <range start="10.0.2.10" end="10.0.2.50"/>
    </dhcp>
  </ip>
</network>
EOF

for net in /tmp/net-office.xml /tmp/net-servers.xml; do
  name=$(grep -oP '(?<=<name>).*(?=</name>)' "$net")
  virsh net-define "$net"
  virsh net-start "$name"
  echo "  Network $name: started"
done

# ── Router VM (3 interfaces) ────────────────────────────────────
echo "Creating router VM..."
create_overlay "$BASE_DEB" "${POOL}/${PREFIX}-router.qcow2"

virt-install \
  --name "${PREFIX}-router" \
  --ram 2048 --vcpus 2 \
  --disk "path=${POOL}/${PREFIX}-router.qcow2,format=qcow2" \
  --import \
  --os-variant debian12 \
  --network network=default \
  --network network=lab-b09-office \
  --network network=lab-b09-servers \
  --graphics none \
  --noautoconsole \
  --quiet

echo "  lab-b09-router: created (3 interfaces)"

# ── Office host ──────────────────────────────────────────────────
echo "Creating office host..."
create_overlay "$BASE_DEB" "${POOL}/${PREFIX}-office.qcow2"

virt-install \
  --name "${PREFIX}-office" \
  --ram 1024 --vcpus 1 \
  --disk "path=${POOL}/${PREFIX}-office.qcow2,format=qcow2" \
  --import \
  --os-variant debian12 \
  --network network=lab-b09-office \
  --graphics none \
  --noautoconsole \
  --quiet

echo "  lab-b09-office: created (office network only)"

# ── Servers host ─────────────────────────────────────────────────
echo "Creating servers host..."
create_overlay "$BASE_ALMA" "${POOL}/${PREFIX}-servers.qcow2"

virt-install \
  --name "${PREFIX}-servers" \
  --ram 1024 --vcpus 1 \
  --disk "path=${POOL}/${PREFIX}-servers.qcow2,format=qcow2" \
  --import \
  --os-variant almalinux9 \
  --network network=lab-b09-servers \
  --graphics none \
  --noautoconsole \
  --quiet

echo "  lab-b09-servers: created (servers network only)"

echo ""
echo "=== B09 lab ready ==="
echo "VMs:"
virsh list --all | grep "$PREFIX"
echo ""
echo "Networks:"
virsh net-list | grep "$PREFIX"
echo ""
echo "After VMs boot, configure the router:"
echo "  virsh console ${PREFIX}-router"
echo "  # Enable IP forwarding: sysctl -w net.ipv4.ip_forward=1"
```

### 3.4. Script de destrucción

```bash
#!/bin/bash
# destroy-lab-b09.sh — Teardown B09 lab
set -euo pipefail
source "$(dirname "$0")/lab-functions.sh"

PREFIX="lab-b09"

echo "=== Destroying B09 lab ==="

# Destroy VMs
for vm in router office servers; do
  destroy_vm "${PREFIX}-${vm}"
  cleanup_disks "${PREFIX}-${vm}" 0
done

# Destroy networks
for net in lab-b09-office lab-b09-servers; do
  if virsh net-info "$net" &>/dev/null; then
    virsh net-destroy "$net" 2>/dev/null || true
    virsh net-undefine "$net" 2>/dev/null || true
    echo "  Network $net: destroyed"
  fi
done

echo "=== B09 lab destroyed ==="
```

---

## 4. B10 — Servicios

### 4.1. Qué necesita

| Componente | Detalle |
|------------|---------|
| VMs | 2-3: servidor Debian + servidor Alma + cliente Debian |
| Discos extra | Ninguno |
| Redes | default (NAT) + 1 red aislada (servicios) |
| Propósito | DNS, DHCP, web, mail — el servidor controla la red |

### 4.2. Diagrama

```
┌──────────────────────────────────────────────────────────────────┐
│                       ENTORNO B10                               │
│                                                                  │
│  ┌─────────────────┐  ┌─────────────────┐  ┌────────────────┐   │
│  │lab-b10-srv-deb  │  │lab-b10-srv-alma │  │lab-b10-client  │   │
│  │  Debian 12      │  │  AlmaLinux 9    │  │  Debian 12     │   │
│  │  2GB / 2 vCPUs  │  │  2GB / 2 vCPUs  │  │  1GB / 1 vCPU  │   │
│  │                 │  │                 │  │                │   │
│  │  DNS, DHCP      │  │  Web, Mail      │  │  Pruebas       │   │
│  │  (controla red) │  │                 │  │  de servicios  │   │
│  └──┬──────────┬───┘  └──┬──────────┬───┘  └──┬─────────┬──┘   │
│     │          │         │          │         │         │       │
│  ┌──▼──┐   ┌───▼─────────▼──────────▼─────────▼──┐      │       │
│  │deflt│   │         lab-b10-services             │      │       │
│  │(NAT)│   │         10.0.10.0/24                 │      │       │
│  │     │   │         (aislada — el servidor DNS   │      │       │
│  └──▲──┘   │          controla nombres y DHCP)    │      │       │
│     │      └──────────────────────────────────────┘      │       │
│     │                                                    │       │
│     └────────────────────────────────────────────────────┘       │
│     NAT solo en servidores (para instalar paquetes)              │
│     El cliente solo tiene la red aislada                         │
└──────────────────────────────────────────────────────────────────┘
```

### 4.3. Script de creación

```bash
#!/bin/bash
# create-lab-b10.sh — B10 Services lab environment
set -euo pipefail
source "$(dirname "$0")/lab-functions.sh"

PREFIX="lab-b10"
BASE_DEB="${POOL}/base-debian12.qcow2"
BASE_ALMA="${POOL}/base-alma9.qcow2"

echo "=== Creating B10 lab (Services) ==="

# ── Create service network (isolated, no DHCP — server will provide it) ──
cat > /tmp/net-services.xml << 'EOF'
<network>
  <name>lab-b10-services</name>
  <bridge name="virbr-svc"/>
  <ip address="10.0.10.1" netmask="255.255.255.0"/>
</network>
EOF

virsh net-define /tmp/net-services.xml
virsh net-start lab-b10-services
echo "  Network lab-b10-services: started (no DHCP — server provides it)"

# ── Debian server (DNS, DHCP) ────────────────────────────────────
echo "Creating Debian server..."
create_overlay "$BASE_DEB" "${POOL}/${PREFIX}-srv-deb.qcow2"

virt-install \
  --name "${PREFIX}-srv-deb" \
  --ram 2048 --vcpus 2 \
  --disk "path=${POOL}/${PREFIX}-srv-deb.qcow2,format=qcow2" \
  --import \
  --os-variant debian12 \
  --network network=default \
  --network network=lab-b10-services \
  --graphics none \
  --noautoconsole \
  --quiet

echo "  ${PREFIX}-srv-deb: created (default + services network)"

# ── AlmaLinux server (Web, Mail) ─────────────────────────────────
echo "Creating AlmaLinux server..."
create_overlay "$BASE_ALMA" "${POOL}/${PREFIX}-srv-alma.qcow2"

virt-install \
  --name "${PREFIX}-srv-alma" \
  --ram 2048 --vcpus 2 \
  --disk "path=${POOL}/${PREFIX}-srv-alma.qcow2,format=qcow2" \
  --import \
  --os-variant almalinux9 \
  --network network=default \
  --network network=lab-b10-services \
  --graphics none \
  --noautoconsole \
  --quiet

echo "  ${PREFIX}-srv-alma: created (default + services network)"

# ── Client (only services network) ──────────────────────────────
echo "Creating client..."
create_overlay "$BASE_DEB" "${POOL}/${PREFIX}-client.qcow2"

virt-install \
  --name "${PREFIX}-client" \
  --ram 1024 --vcpus 1 \
  --disk "path=${POOL}/${PREFIX}-client.qcow2,format=qcow2" \
  --import \
  --os-variant debian12 \
  --network network=lab-b10-services \
  --graphics none \
  --noautoconsole \
  --quiet

echo "  ${PREFIX}-client: created (services network only)"

echo ""
echo "=== B10 lab ready ==="
echo "VMs:"
virsh list --all | grep "$PREFIX"
echo ""
echo "The client has NO internet access."
echo "It depends on the Debian server for DNS/DHCP."
echo "Configure the server first:"
echo "  virsh console ${PREFIX}-srv-deb"
```

### 4.4. Script de destrucción

```bash
#!/bin/bash
# destroy-lab-b10.sh — Teardown B10 lab
set -euo pipefail
source "$(dirname "$0")/lab-functions.sh"

PREFIX="lab-b10"

echo "=== Destroying B10 lab ==="

for vm in srv-deb srv-alma client; do
  destroy_vm "${PREFIX}-${vm}"
  cleanup_disks "${PREFIX}-${vm}" 0
done

if virsh net-info lab-b10-services &>/dev/null; then
  virsh net-destroy lab-b10-services 2>/dev/null || true
  virsh net-undefine lab-b10-services 2>/dev/null || true
  echo "  Network lab-b10-services: destroyed"
fi

echo "=== B10 lab destroyed ==="
```

### 4.5. Diseño deliberado: cliente sin internet

El cliente solo tiene interfaz en la red aislada `lab-b10-services`. No tiene
red NAT. Esto es intencional:

```
¿Por qué el cliente no tiene internet?

1. Fuerza a que el servidor DHCP funcione — sin él, el cliente no tiene IP
2. Fuerza a que el servidor DNS funcione — sin él, el cliente no resuelve nombres
3. Simula un entorno real: los clientes de una LAN dependen de infraestructura

Si algo falla en el servidor, el cliente lo refleja inmediatamente.
Es la mejor forma de verificar que los servicios funcionan.
```

---

## 5. B11 — Seguridad, Kernel y Boot

### 5.1. Qué necesita

| Componente | Detalle |
|------------|---------|
| VMs | 1 AlmaLinux 9 (SELinux) + 1 Debian 12 (AppArmor) |
| Discos extra | 1 × 2G en Alma (para LUKS) |
| Redes | default (NAT) |
| Snapshots | Antes de cada lab de GRUB/boot |

### 5.2. Diagrama

```
┌──────────────────────────────────────────────────────────────────┐
│                       ENTORNO B11                               │
│                                                                  │
│  ┌──────────────────────┐    ┌──────────────────────┐            │
│  │  lab-b11-alma        │    │  lab-b11-debian      │            │
│  │  AlmaLinux 9         │    │  Debian 12           │            │
│  │  2GB RAM / 2 vCPUs   │    │  2GB RAM / 2 vCPUs   │            │
│  │                      │    │                      │            │
│  │  Labs:               │    │  Labs:               │            │
│  │  - SELinux           │    │  - AppArmor          │            │
│  │  - LUKS (vdb: 2G)    │    │  - GRUB/Boot         │            │
│  │  - Firewalld         │    │  - Kernel modules    │            │
│  │  - GRUB/Boot         │    │  - systemd           │            │
│  └────────┬─────────────┘    └────────┬─────────────┘            │
│           └──────────┬───────────────┘                           │
│                      │                                           │
│              ┌───────▼────────┐                                  │
│              │    default     │                                  │
│              │     (NAT)      │                                  │
│              └────────────────┘                                  │
│                                                                  │
│  ⚠ SNAPSHOTS OBLIGATORIOS:                                       │
│    Antes de cada lab de GRUB/boot (recovery si se rompe el boot) │
│    snapshot-create-as lab-b11-alma "pre-grub"                    │
│    snapshot-create-as lab-b11-debian "pre-grub"                  │
└──────────────────────────────────────────────────────────────────┘
```

### 5.3. Script de creación

```bash
#!/bin/bash
# create-lab-b11.sh — B11 Security/Kernel/Boot lab environment
set -euo pipefail
source "$(dirname "$0")/lab-functions.sh"

PREFIX="lab-b11"
BASE_DEB="${POOL}/base-debian12.qcow2"
BASE_ALMA="${POOL}/base-alma9.qcow2"

echo "=== Creating B11 lab (Security/Kernel/Boot) ==="

# ── AlmaLinux VM (SELinux, LUKS, firewalld) ──────────────────────
echo "Creating AlmaLinux VM..."
create_overlay "$BASE_ALMA" "${POOL}/${PREFIX}-alma.qcow2"
create_vm "${PREFIX}-alma" "${POOL}/${PREFIX}-alma.qcow2" 2048 2 almalinux9
attach_lab_disks "${PREFIX}-alma" 1 2G
echo "  ${PREFIX}-alma: created with 1 extra disk (for LUKS)"

# ── Debian VM (AppArmor, GRUB, kernel) ───────────────────────────
echo "Creating Debian VM..."
create_overlay "$BASE_DEB" "${POOL}/${PREFIX}-debian.qcow2"
create_vm "${PREFIX}-debian" "${POOL}/${PREFIX}-debian.qcow2" 2048 2 debian12
echo "  ${PREFIX}-debian: created (no extra disks)"

# ── Create initial snapshots ────────────────────────────────────
echo "Waiting for VMs to boot (15s)..."
sleep 15

echo "Creating safety snapshots..."
virsh snapshot-create-as "${PREFIX}-alma" "clean-state" \
  --description "Clean state before any labs" 2>/dev/null || true
virsh snapshot-create-as "${PREFIX}-debian" "clean-state" \
  --description "Clean state before any labs" 2>/dev/null || true
echo "  Snapshots 'clean-state' created for both VMs"

echo ""
echo "=== B11 lab ready ==="
echo "VMs:"
virsh list --all | grep "$PREFIX"
echo ""
echo "IMPORTANT: Before GRUB/boot labs, create a snapshot:"
echo "  virsh snapshot-create-as ${PREFIX}-alma pre-grub"
echo "  virsh snapshot-create-as ${PREFIX}-debian pre-grub"
echo ""
echo "If boot breaks, revert:"
echo "  virsh snapshot-revert ${PREFIX}-alma pre-grub --running"
```

### 5.4. Script de destrucción

```bash
#!/bin/bash
# destroy-lab-b11.sh — Teardown B11 lab
set -euo pipefail
source "$(dirname "$0")/lab-functions.sh"

PREFIX="lab-b11"

echo "=== Destroying B11 lab ==="

# Delete snapshots first (required before undefine with snapshots)
for vm in alma debian; do
  full="${PREFIX}-${vm}"
  if virsh dominfo "$full" &>/dev/null; then
    for snap in $(virsh snapshot-list "$full" --name 2>/dev/null); do
      virsh snapshot-delete "$full" "$snap" 2>/dev/null || true
    done
  fi
done

destroy_vm "${PREFIX}-alma"
cleanup_disks "${PREFIX}-alma" 1

destroy_vm "${PREFIX}-debian"
cleanup_disks "${PREFIX}-debian" 0

echo "=== B11 lab destroyed ==="
```

### 5.5. Patrón de snapshots para labs de boot

Los labs de GRUB y boot son los más peligrosos — un error puede dejar la VM
sin arrancar. La solución son snapshots:

```
┌─────────────────────────────────────────────────────────────┐
│           PATRÓN DE SNAPSHOTS PARA LABS DE BOOT             │
│                                                             │
│  1. snapshot-create-as VM "pre-grub"                        │
│                                                             │
│  2. Hacer el lab (modificar GRUB, kernel params, etc.)      │
│                                                             │
│  3a. Si todo funciona:                                      │
│      snapshot-create-as VM "post-grub" (nuevo checkpoint)   │
│                                                             │
│  3b. Si el boot se rompe:                                   │
│      snapshot-revert VM "pre-grub" --running                │
│      (vuelve al estado anterior, como si nada pasó)         │
│                                                             │
│  Timeline:                                                  │
│  ─────●──────────────●──────────────●─────────              │
│    clean-state    pre-grub       post-grub                  │
│    (inicio)       (antes lab)    (lab exitoso)              │
└─────────────────────────────────────────────────────────────┘
```

---

## 6. Organización de scripts

### 6.1. Estructura de directorio recomendada

```
~/lab-scripts/
├── lab-functions.sh             # funciones reutilizables (T01)
├── create-lab-b08.sh            # provisioning B08
├── destroy-lab-b08.sh           # teardown B08
├── create-lab-b09.sh            # provisioning B09
├── destroy-lab-b09.sh           # teardown B09
├── create-lab-b10.sh            # provisioning B10
├── destroy-lab-b10.sh           # teardown B10
├── create-lab-b11.sh            # provisioning B11
├── destroy-lab-b11.sh           # teardown B11
└── verify-lab.sh                # verificación genérica
```

```bash
# Crear la estructura
mkdir -p ~/lab-scripts
chmod +x ~/lab-scripts/*.sh
```

### 6.2. Flujo de trabajo entre bloques

```bash
# Empezando B08:
sudo ~/lab-scripts/create-lab-b08.sh

# ... labs de B08 ...

# Terminando B08, empezando B09:
sudo ~/lab-scripts/destroy-lab-b08.sh
sudo ~/lab-scripts/create-lab-b09.sh

# ... labs de B09 ...

# Y así sucesivamente
```

No es necesario destruir B08 para crear B09 (las VMs tienen nombres distintos),
pero destruir libera RAM y disco. Si tu máquina tiene suficientes recursos
puedes tener varios entornos simultáneos.

### 6.3. Resumen de recursos por bloque

| Bloque | VMs | RAM total | Discos extra | Redes extra |
|--------|-----|-----------|--------------|-------------|
| B08 | 2 | 4 GB | 8 × 2G = 16G virtual | 0 |
| B09 | 3 | 4 GB | 0 | 2 |
| B10 | 3 | 5 GB | 0 | 1 |
| B11 | 2 | 4 GB | 1 × 2G | 0 |

RAM real necesaria: ~4-5 GB libres (un bloque a la vez). Los discos son thin
provisioning, así que los 16G de B08 ocupan apenas unos KB hasta que se usan.

---

## 7. Errores comunes

### Error 1: ejecutar create de dos bloques sin suficiente RAM

```bash
# ✗ B08 (4GB) + B09 (4GB) = 8GB de RAM para VMs
# Si tu host tiene 8GB total, no queda RAM para el SO host
sudo ./create-lab-b08.sh
sudo ./create-lab-b09.sh    # VMs no arrancan o el host se congela

# ✓ Destruir un bloque antes de crear otro
sudo ./destroy-lab-b08.sh
sudo ./create-lab-b09.sh

# ✓ O reducir RAM en los scripts si tu host es limitado
# Cambiar 2048 → 1024 en VMs que no necesitan mucha RAM
```

### Error 2: olvidar destruir redes en B09/B10

```bash
# ✗ destroy-lab que solo destruye VMs pero no las redes
virsh net-list --all | grep lab-b09
# lab-b09-office    active
# lab-b09-servers   active
# Las redes quedan huérfanas

# ✓ Los scripts de destroy incluyen limpieza de redes
virsh net-destroy lab-b09-office
virsh net-undefine lab-b09-office
```

### Error 3: no hacer snapshot antes de lab de boot (B11)

```bash
# ✗ Modificar GRUB sin snapshot
# La VM no arranca → tienes que recrearla desde cero

# ✓ Siempre snapshot antes de tocar GRUB/boot
virsh snapshot-create-as lab-b11-debian "pre-grub"
# ... lab ...
# Si se rompe:
virsh snapshot-revert lab-b11-debian "pre-grub" --running
```

### Error 4: imágenes base con permisos de escritura

```bash
# ✗ base-debian12.qcow2 con permisos 644
# Un script con error podría sobrescribirla

# ✓ Proteger las imágenes base
sudo chmod 444 /var/lib/libvirt/images/base-debian12.qcow2
sudo chmod 444 /var/lib/libvirt/images/base-alma9.qcow2
# Ahora ni siquiera root las modifica accidentalmente
```

### Error 5: red con nombre duplicado

```bash
# ✗ Crear lab-b09 dos veces sin destruir
virsh net-define /tmp/net-office.xml
# Error: "network 'lab-b09-office' already exists"

# ✓ Verificar primero o usar el script de destroy
sudo ./destroy-lab-b09.sh
sudo ./create-lab-b09.sh
```

---

## 8. Cheatsheet

```bash
# ── Levantar un entorno ──────────────────────────────────────────
sudo ~/lab-scripts/create-lab-b08.sh
sudo ~/lab-scripts/create-lab-b09.sh
sudo ~/lab-scripts/create-lab-b10.sh
sudo ~/lab-scripts/create-lab-b11.sh

# ── Destruir un entorno ──────────────────────────────────────────
sudo ~/lab-scripts/destroy-lab-b08.sh
sudo ~/lab-scripts/destroy-lab-b09.sh
sudo ~/lab-scripts/destroy-lab-b10.sh
sudo ~/lab-scripts/destroy-lab-b11.sh

# ── Verificar ────────────────────────────────────────────────────
~/lab-scripts/verify-lab.sh lab-b08

# ── Cambiar de bloque ────────────────────────────────────────────
sudo ~/lab-scripts/destroy-lab-b08.sh && \
sudo ~/lab-scripts/create-lab-b09.sh

# ── Snapshots para labs peligrosos (B11) ─────────────────────────
virsh snapshot-create-as VM "checkpoint-name"
# ... hacer el lab ...
virsh snapshot-revert VM "checkpoint-name" --running

# ── Recursos por bloque ─────────────────────────────────────────
# B08: 2 VMs, 4GB RAM, 8 discos extra, red default
# B09: 3 VMs, 4GB RAM, 0 discos, 2 redes aisladas
# B10: 3 VMs, 5GB RAM, 0 discos, 1 red aislada
# B11: 2 VMs, 4GB RAM, 1 disco, red default + snapshots

# ── Estructura de scripts ───────────────────────────────────────
# ~/lab-scripts/
#   lab-functions.sh        funciones compartidas
#   create-lab-bNN.sh       provisioning
#   destroy-lab-bNN.sh      teardown
#   verify-lab.sh           verificación
```

---

## 9. Ejercicios

### Ejercicio 1: implementar la receta de B08

1. Crea el directorio `~/lab-scripts/`
2. Copia `lab-functions.sh` de T01 (sección 6.1)
3. Crea `create-lab-b08.sh` y `destroy-lab-b08.sh` basándote en la sección 2
4. Ejecuta `create-lab-b08.sh` y verifica:
   - Ambas VMs corren (`virsh list`)
   - Cada una tiene 4 discos extra (`virsh domblklist`)
   - Puedes acceder a ellas (`virsh console`)
   - Dentro de la VM, `lsblk` muestra vdb-vde
5. Ejecuta `destroy-lab-b08.sh` y verifica que no queda nada

**Pregunta de reflexión**: los 8 discos extra (4 por VM, 2G cada uno) representan
16G de espacio virtual. ¿Cuánto espacio real ocupa esto en el host justo después
de crear el entorno? ¿Y después de hacer un lab de RAID5 en una VM?

### Ejercicio 2: implementar la receta de B09

1. Crea `create-lab-b09.sh` y `destroy-lab-b09.sh`
2. Ejecuta el create y verifica:
   - 3 VMs corren
   - El router tiene 3 interfaces (`virsh domiflist lab-b09-router`)
   - Las redes aisladas existen (`virsh net-list`)
3. Dentro del router, habilita ip_forward y verifica conectividad entre subnets
4. Ejecuta el destroy y verifica que VMs Y redes se eliminaron

**Pregunta de reflexión**: el script de B09 no usa `create_vm` de
`lab-functions.sh` para el router porque necesita 3 interfaces. ¿Cómo
modificarías la función `create_vm` para soportar múltiples redes? ¿Vale la
pena la complejidad adicional o es mejor dejarlo como caso especial?

### Ejercicio 3: snapshots de seguridad para B11

1. Crea `create-lab-b11.sh` con snapshot inicial automático
2. Ejecútalo y verifica que el snapshot `clean-state` existe:
   `virsh snapshot-list lab-b11-debian`
3. Dentro de la VM Debian, haz un cambio en GRUB (por ejemplo, cambiar el
   timeout a 10 segundos)
4. Reinicia la VM y verifica que el cambio se aplicó
5. Revierte al snapshot: `virsh snapshot-revert lab-b11-debian clean-state --running`
6. Verifica que el cambio de GRUB desapareció

**Pregunta de reflexión**: el snapshot `clean-state` se crea 15 segundos después
de arrancar las VMs. ¿Es suficiente tiempo? ¿Qué pasa si la VM no terminó de
arrancar cuando se toma el snapshot? ¿Cómo mejorarías esto?
