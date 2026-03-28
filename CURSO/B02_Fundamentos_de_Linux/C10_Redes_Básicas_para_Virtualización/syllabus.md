# Capítulo 10: Redes Básicas para Virtualización

**Objetivo**: Dominar los conceptos mínimos de red IP necesarios para configurar y entender
QEMU/KVM y libvirt. No es un estudio exhaustivo de redes — es el conocimiento necesario y
suficiente para el capítulo C00 (QEMU/KVM) y bloques B08–B11.

**Prerrequisitos**: C02 (permisos), C03 (procesos), C06 (systemd), C09 (monitoreo básico).

## Sección 1: Direcciones IP

### T01 — Direcciones IPv4
- Formato: 4 octetos, 0–255 cada uno (ej: 192.168.1.100)
- Notación decimal punteada vs binario
- Tipos de dirección:
  - **Unicast**: una sola máquina destino
  - **Broadcast**: todas las máquinas en la red (ej: 192.168.1.255)
  - **Loopback**: 127.0.0.1 (localhost), ::1 (IPv6)
- Ver tu IP: `ip addr`, `hostname -I`
- Ejercicio: identifica la IP de tu máquina en la red local

### T02 — Máscara de Red y CIDR
- Qué es una máscara: indica qué porción de la IP es la red y cuál es el host
- Máscara tradicional: 255.255.255.0 (equivalente a /24)
- CIDR (Classless Inter-Domain Routing): /24, /16, /8 — notación moderna
- Ejemplos:
  - 192.168.1.0/24 → 256 direcciones (254 usables para hosts)
  - 10.0.0.0/8 → 16M direcciones (red privada grande)
- Cálculo rápido: /24 = 256 IPs, /25 = 128, /26 = 64, /27 = 32
- `ipcalc` para calcular rangos: `ipcalc 192.168.1.0/24`

### T03 — Direcciones Privadas y Loopback
- Rangos privados (no ruteables en internet):
  - 10.0.0.0/8 — red privada grande
  - 172.16.0.0/12 — red privada media
  - 192.168.0.0/16 — red privada pequeña (la más común en hogaresoficinas)
- NAT: las máquinas con IP privada acceden a internet a través de un gateway
- Loopback: 127.0.0.1 — la propia máquina (no requiere red física)
- Ejercicio: verifica qué rango usa tu red WiFi doméstica

## Sección 2: NAT y DHCP

### T01 — NAT (Network Address Translation)
- Problema: las IP privadas no son ruteables en internet
- Solución: el gateway traduce IP privada → IP pública (del router)
- Tipos:
  - SNAT: Source NAT — modifica la IP origen saliente
  - DNAT: Destination NAT — modifica la IP destino entrante
  - MASQUERADE: SNAT automático (el gateway usa su propia IP pública)
- En QEMU/KVM: la red `default` de libvirt usa NAT — las VMs acceden a internet a través del host
- Diagrama: VM (192.168.122.100) → Host (NAT) → Internet

### T02 — DHCP (Dynamic Host Configuration Protocol)
- Problema: asignar IPs manualmente a cada máquina es tedioso
- Solución: un servidor DHCP asigna IPs automáticamente
- Proceso DORA: Discover → Offer → Request → Acknowledge
- En libvirt: el demonio libvirt incluye un servidor DHCP para la red NAT
- Ver leases DHCP de libvirt: `virsh net-dhcp-leases default`
- Renew de lease: `dhclient -r` (release) y `dhclient` (renew)
- Configuración en `/etc/dhcp/dhclient.conf` (Debian) o `/etc/sysconfig/network-scripts/`

### T03 — DNS Básico
- Problema: los humanos no recuerdan IPs, sí recuerdan nombres (google.com)
- DNS: traduce nombres de dominio a IPs
- Servidores DNS: se configuran en `/etc/resolv.conf` (ej: 8.8.8.8 de Google)
- Consulta DNS: `dig google.com`, `nslookup google.com`
- `/etc/hosts`: mapeo manual nombre → IP (override local de DNS)
- En VMs de lab: обычно el router (192.168.122.1) es también el DNS para la red NAT de libvirt

## Sección 3: Bridge Networking

### T01 — Concepto de Bridge (Puente)
- Bridge (puente): conecta dos redes como si fueran una sola — opera en capa 2 (Ethernet)
- Switch vs Bridge: prácticamente son sinónimos en redes modernas
- Bridge en Linux: software que convierte al host en un switch virtual
- Uso en virtualización: las VMs conectadas a un bridge aparecen en la misma red física que el host
- Comparación rápida:
  - NAT: la VM está "detrás" del host, no visible desde la red
  - Bridge: la VM es como una máquina física conectada al mismo switch

### T02 — Bridge en Linux
- Paquetes: `bridge-utils` (comandos clásicos) o el soporte integrado en `iproute2`
- Crear bridge: `ip link add br0 type bridge`
- Añadir interfaz física al bridge: `ip link set eth0 master br0`
- Eliminar interfaz: `ip link set eth0 nomaster`
- Ver bridges: `bridge link`, `brctl show` (si está instalado)
- En libvirt: `<interface type='bridge'><source bridge='br0'/>`
- Ejercicio: crear un bridge manual entre dos interfaces en el host

## Sección 4: Redes Virtuales en libvirt

### T01 — Tipos de Redes en libvirt
- **NAT (default)**: 192.168.122.0/24, DHCP, las VMs acceden a internet a través del host
- **Aislada (isolated)**: red privada sin NAT ni acceso a internet — solo VMs entre sí
- **Bridge**: usa un bridge del host para conectar VMs a la red física
- **Redirección de puertos (port forwarding)**: redirigir puertos del host a la VM
- Comparación:
  | Tipo | VM → Host | VM → Internet | VM → VM |
  |---|---|---|---|
  | NAT | ✅ | ✅ | ✅ |
  | Aislada | ❌ | ❌ | ✅ |
  | Bridge | ✅ | ✅ | ✅ |

### T02 — Gestión de Redes Virtuales
- Listar redes: `virsh net-list --all`
- Ver información de red: `virsh net-info default`
- Ver leases DHCP: `virsh net-dhcp-leases default`
- Iniciar/detener red: `virsh net-start <red>`, `virsh net-destroy <red>`
- Autostart: `virsh net-autostart <red>`
- XML de red: `virsh net-edit <red>` — permite modificar parámetros
- Ver XML actual: `virsh net-dumpxml <red>`
- En el curso B08: redes aisladas se usan para labs de almacenamiento/RAID sin riesgo
- En B09/B10: se configuran topologías multi-VM (router + hosts)
