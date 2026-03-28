# Concepto de Bridge (Puente de Red)

## Índice

1. [El problema: las VMs detrás de NAT son invisibles](#1-el-problema-las-vms-detrás-de-nat-son-invisibles)
2. [Qué es un bridge](#2-qué-es-un-bridge)
3. [Capa 2: Ethernet, MACs y tramas](#3-capa-2-ethernet-macs-y-tramas)
4. [Cómo funciona un bridge: la tabla MAC](#4-cómo-funciona-un-bridge-la-tabla-mac)
5. [Bridge vs hub vs switch vs router](#5-bridge-vs-hub-vs-switch-vs-router)
6. [Bridge en virtualización: el switch virtual](#6-bridge-en-virtualización-el-switch-virtual)
7. [Bridge vs NAT: comparación detallada](#7-bridge-vs-nat-comparación-detallada)
8. [Spanning Tree Protocol (STP)](#8-spanning-tree-protocol-stp)
9. [Tipos de bridge en Linux](#9-tipos-de-bridge-en-linux)
10. [Escenarios: cuándo usar bridge y cuándo no](#10-escenarios-cuándo-usar-bridge-y-cuándo-no)
11. [El bridge en el contexto del curso](#11-el-bridge-en-el-contexto-del-curso)
12. [Errores comunes](#12-errores-comunes)
13. [Cheatsheet](#13-cheatsheet)
14. [Ejercicios](#14-ejercicios)

---

## 1. El problema: las VMs detrás de NAT son invisibles

Con la red NAT de libvirt (192.168.122.0/24), las VMs pueden acceder a internet, pero nadie puede acceder a ellas desde fuera del host:

```
Laptop de un compañero (192.168.1.200)
    │
    │  quiere acceder a nginx en la VM
    │  "curl http://192.168.122.100"  ← FALLA
    │  (192.168.122.100 no existe en la LAN)
    │
    ↓
Router (192.168.1.1) → no sabe qué es 192.168.122.x → descarta
```

La VM tiene una IP en una red privada (192.168.122.0/24) que solo existe dentro del host. Para que la VM sea visible en la LAN del host, necesitamos conectarla directamente a esa LAN. Eso es lo que hace un bridge.

```
Con bridge:
Laptop del compañero (192.168.1.200)
    │
    │  "curl http://192.168.1.150"  ← FUNCIONA
    │  (la VM tiene una IP en la LAN real)
    │
    ↓
Router (192.168.1.1) → DHCP asigna 192.168.1.150 a la VM → OK
```

---

## 2. Qué es un bridge

Un bridge (puente) es un dispositivo que **conecta dos o más segmentos de red y los hace funcionar como una sola red** a nivel de capa 2 (Ethernet). En Linux, un bridge es una implementación por software que convierte al host en un switch virtual.

### Analogía

Imagina dos habitaciones con personas que quieren comunicarse:

- **Sin bridge**: cada habitación es una red aislada. Las personas de una habitación no pueden hablar con las de la otra.
- **Con bridge**: abrimos una puerta entre las dos habitaciones. Ahora todas las personas pueden comunicarse como si estuvieran en la misma habitación.

En virtualización:
- **Habitación 1**: la red física del host (tu LAN, con router, laptops, etc.).
- **Habitación 2**: las VMs.
- **Bridge**: la "puerta" que conecta ambas. Las VMs aparecen en la LAN como si fueran máquinas físicas.

### Definición técnica

Un bridge examina cada **trama Ethernet** (capa 2) que recibe, lee la **dirección MAC destino**, y decide si debe reenviarla por otro puerto o descartarla. Aprende qué MACs están en qué puerto observando el tráfico, construyendo una **tabla de MACs** (forwarding database / FDB).

---

## 3. Capa 2: Ethernet, MACs y tramas

Para entender el bridge, necesitas entender en qué capa opera.

### El modelo OSI relevante

```
Capa 3 — Red (IP)         Router decide por IP
                           ───────────────────
Capa 2 — Enlace (Ethernet) Bridge/Switch decide por MAC  ← AQUÍ
                           ───────────────────
Capa 1 — Física            Cable, señales eléctricas
```

Un bridge **no entiende de IPs**. Solo ve MACs. No sabe que 192.168.1.100 existe — solo sabe que la MAC `08:00:27:ab:cd:ef` está en el puerto 1.

### Trama Ethernet

Cada paquete que viaja por una red Ethernet va envuelto en una **trama**:

```
┌──────────────────────────────────────────────────────────┐
│ MAC destino │ MAC origen │ Tipo │ Datos (payload) │ FCS  │
│ (6 bytes)   │ (6 bytes)  │(2 B) │ (46-1500 bytes) │(4 B)│
└──────────────────────────────────────────────────────────┘
  ──────────────────────────
  Esto es lo que el bridge lee
  para tomar decisiones
```

| Campo | Significado |
|-------|------------|
| MAC destino | ¿A quién va dirigida la trama? |
| MAC origen | ¿Quién la envió? (el bridge aprende de esto) |
| Tipo | Qué protocolo va dentro (0x0800 = IPv4, 0x0806 = ARP) |
| Datos | El paquete IP completo (con IPs, puertos, etc.) |
| FCS | Frame Check Sequence (verificación de integridad) |

### Direcciones MAC

Las direcciones MAC (Media Access Control) son identificadores de 48 bits asignados a cada interfaz de red:

```
08:00:27:ab:cd:ef
──────── ────────
OUI      NIC-specific
(fabricante) (dispositivo)
```

| OUI (primeros 3 bytes) | Fabricante |
|------------------------|-----------|
| 08:00:27 | VirtualBox |
| 52:54:00 | QEMU/KVM |
| 00:0c:29 | VMware |
| 02:42:xx | Docker |

Cuando ves una MAC que empieza con `52:54:00`, sabes que es una interfaz de QEMU/KVM.

---

## 4. Cómo funciona un bridge: la tabla MAC

El bridge toma decisiones de reenvío basándose en una tabla que construye observando el tráfico. El proceso se llama **MAC learning**.

### Paso 1: estado inicial — tabla vacía

```
Bridge br0 (tabla vacía)
  Puerto 1: eth0 (interfaz física del host)
  Puerto 2: vnet0 (VM1)
  Puerto 3: vnet1 (VM2)

Tabla MAC:
  (vacía — aún no ha visto tráfico)
```

### Paso 2: VM1 envía una trama

```
VM1 (MAC: 52:54:00:aa:11:11) envía una trama a cualquier destino

Bridge ve:
  "La trama llegó por puerto 2, origen MAC 52:54:00:aa:11:11"
  → Aprender: 52:54:00:aa:11:11 está en puerto 2

Tabla MAC:
  52:54:00:aa:11:11 → Puerto 2

Como no conoce el destino, FLOOD:
  → Reenvía la trama por TODOS los puertos (excepto el de origen)
  → Puerto 1 (eth0) y Puerto 3 (vnet1) reciben la trama
```

### Paso 3: el destino responde

```
La máquina destino (MAC: 08:00:27:bb:22:22) responde por puerto 1 (eth0)

Bridge ve:
  "Trama desde puerto 1, origen 08:00:27:bb:22:22, destino 52:54:00:aa:11:11"
  → Aprender: 08:00:27:bb:22:22 está en puerto 1
  → Destino 52:54:00:aa:11:11 está en puerto 2 (ya lo sabe)
  → Reenviar SOLO por puerto 2 (no hace flood)

Tabla MAC:
  52:54:00:aa:11:11  → Puerto 2
  08:00:27:bb:22:22  → Puerto 1
```

### Paso 4: estado estable

Después de un poco de tráfico, el bridge conoce todas las MACs:

```
Tabla MAC:
  52:54:00:aa:11:11  → Puerto 2 (VM1)
  52:54:00:aa:22:22  → Puerto 3 (VM2)
  08:00:27:bb:22:22  → Puerto 1 (laptop en la LAN)
  d4:3b:04:cc:33:33  → Puerto 1 (teléfono en la LAN)

Ahora el bridge reenvía tramas de forma selectiva:
  VM1 → VM2: solo por puerto 3 (no sale por eth0)
  VM1 → laptop: solo por puerto 1 (no va a VM2)
  laptop → VM2: solo por puerto 3
```

### Aging: las entradas expiran

Las entradas de la tabla MAC tienen un **aging timer** (típicamente 300 segundos). Si una MAC no genera tráfico en ese tiempo, se elimina de la tabla. Si vuelve a aparecer, se reaprende.

```bash
# Ver el aging time del bridge
cat /sys/class/net/br0/bridge/ageing_time
# 30000  (en centésimas de segundo = 300 segundos)
```

---

## 5. Bridge vs hub vs switch vs router

Estos cuatro dispositivos se confunden frecuentemente. La diferencia está en qué capa operan y qué información usan para tomar decisiones:

### Comparación

| Dispositivo | Capa OSI | Unidad de datos | Qué mira | Decisión |
|------------|---------|----------------|----------|----------|
| **Hub** | 1 (Física) | Bits | Nada | Reenvía TODO a TODOS los puertos |
| **Bridge** | 2 (Enlace) | Tramas | MAC destino | Reenvía selectivamente por MAC |
| **Switch** | 2 (Enlace) | Tramas | MAC destino | = Bridge con muchos puertos |
| **Router** | 3 (Red) | Paquetes | IP destino | Conecta redes DIFERENTES |

### Bridge ≈ Switch

En redes modernas, bridge y switch son prácticamente lo mismo. La diferencia es histórica:

- **Bridge** (1980s): conecta 2 segmentos de red. Software. Pocos puertos.
- **Switch** (1990s+): bridge con muchos puertos (8, 24, 48). Hardware dedicado (ASICs).

En Linux, el componente del kernel que implementa un bridge funciona exactamente como un switch: múltiples puertos, tabla MAC, forwarding selectivo. El nombre `bridge` persiste por convención.

### ¿Por qué un bridge y no un router?

Un router conecta redes **diferentes** (192.168.1.0/24 ↔ 10.0.0.0/24). Un bridge conecta segmentos de la **misma red**.

En virtualización:
- **NAT**: el host actúa como router entre la red de VMs (192.168.122.0/24) y la LAN (192.168.1.0/24). Son redes diferentes → routing + NAT.
- **Bridge**: las VMs se conectan directamente a la LAN (192.168.1.0/24). Es la misma red → bridge, sin NAT.

```
NAT (dos redes, router entre ellas):
  VM: 192.168.122.100 ──[NAT/router]── LAN: 192.168.1.0/24

Bridge (una sola red):
  VM: 192.168.1.150 ──[bridge]── LAN: 192.168.1.0/24
  (la VM tiene IP en la misma red que el host)
```

---

## 6. Bridge en virtualización: el switch virtual

En QEMU/KVM, el bridge conecta las interfaces virtuales de las VMs (TAP devices) con la interfaz física del host. El resultado es que las VMs aparecen como máquinas más en la red física.

### Topología con bridge

```
┌───────────────────────── HOST ─────────────────────────────┐
│                                                             │
│  ┌── VM1 ──────┐   ┌── VM2 ──────┐                        │
│  │ eth0         │   │ eth0         │                        │
│  │ (52:54:00:..)│   │ (52:54:00:..)│                        │
│  └──────┬───────┘   └──────┬───────┘                        │
│         │ vnet0             │ vnet1                          │
│         │                   │                                │
│  ┌──────┴───────────────────┴──────┐                        │
│  │          Bridge: br0            │                        │
│  │     (switch virtual de Linux)   │                        │
│  │                                 │                        │
│  │  Tabla MAC:                     │                        │
│  │  52:54:00:aa:11:11 → vnet0     │                        │
│  │  52:54:00:aa:22:22 → vnet1     │                        │
│  │  d4:3b:04:cc:33:33 → enp0s3   │                        │
│  └──────────────┬──────────────────┘                        │
│                 │ enp0s3 (interfaz física)                  │
│                 │ (ya NO tiene IP propia — la IP            │
│                 │  está en br0)                              │
└─────────────────┼───────────────────────────────────────────┘
                  │
                  │ cable Ethernet
                  │
         ┌────────┴────────┐
         │   Switch/Router  │
         │   192.168.1.1    │
         │                  │
         │  DHCP asigna:    │
         │  Host: .50       │  (IP de br0)
         │  VM1:  .150      │
         │  VM2:  .151      │
         └──────────────────┘
```

### Qué pasa con la IP del host

Cuando creas un bridge, la interfaz física (`enp0s3`) se convierte en un **puerto del bridge** y pierde su IP. La IP del host se mueve al bridge (`br0`):

```
ANTES del bridge:
  enp0s3: 192.168.1.50/24  (la interfaz tiene la IP)

DESPUÉS de crear el bridge:
  enp0s3: sin IP              (es un puerto del bridge, no un endpoint)
  br0:    192.168.1.50/24    (el bridge tiene la IP del host)
```

Esto es análogo a cómo funciona un switch físico: el switch no tiene IP en sus puertos — tiene una IP de gestión asignada a una interfaz virtual.

### TAP devices: la conexión VM ↔ bridge

Cada VM conectada al bridge tiene un TAP device (`vnetN`) que es su "cable virtual":

```
VM  ←──→  vnet0  ←──→  br0  ←──→  enp0s3  ←──→  red física
          ──────        ───        ──────
          "cable"       "switch"   "uplink"
          virtual       virtual    (cable real)
```

El TAP device se crea automáticamente cuando la VM arranca y se destruye cuando la VM se apaga:

```bash
# Con una VM corriendo:
ip link show type bridge
# br0: <BROADCAST,MULTICAST,UP,LOWER_UP>

bridge link show
# vnet0: <BROADCAST,MULTICAST,UP,LOWER_UP> master br0
# enp0s3: <BROADCAST,MULTICAST,UP,LOWER_UP> master br0
```

---

## 7. Bridge vs NAT: comparación detallada

### Flujo de un paquete

**Con NAT** (la VM envía un paquete a internet):

```
VM (192.168.122.100) → virbr0 → iptables MASQUERADE → enp0s3 → router
                                 ──────────────────
                                 traduce IP privada a la IP del host
```

El paquete pasa por capa 3 (routing + NAT). La IP de la VM se reemplaza.

**Con bridge** (la VM envía un paquete a internet):

```
VM (192.168.1.150) → vnet0 → br0 → enp0s3 → router
                              ───
                              simple forwarding de capa 2
```

El paquete pasa solo por capa 2 (switching). La IP de la VM no se modifica.

### Tabla comparativa

| Aspecto | NAT | Bridge |
|---------|-----|--------|
| IP de la VM | Red privada (192.168.122.x) | Red del host (192.168.1.x) |
| Visible desde la LAN | No (necesita port forwarding) | Sí (IP en la misma red) |
| Visible desde internet | No (doble port forwarding) | Depende del router |
| Configuración en el host | Ninguna (libvirt default) | Crear bridge, mover IP |
| DHCP de la VM | dnsmasq de libvirt | DHCP del router real |
| DNS de la VM | dnsmasq de libvirt | DNS del router real |
| Rendimiento | Ligera penalización (NAT + conntrack) | Directo (sin traducción) |
| Aislamiento | VM aislada de la LAN | VM expuesta a la LAN |
| IPs disponibles | 254 (pool privado) | Depende del DHCP del router |
| Riesgo de conflictos | Bajo (red privada separada) | Posible (compite con otros hosts) |
| Funciona con WiFi | Sí | Complicado (ver nota) |

### ¿Por qué bridge es complicado con WiFi?

El bridging de capa 2 requiere que la interfaz permita tramas con MACs de origen diferentes a la suya. Las tarjetas WiFi en modo cliente **no permiten esto** — el access point rechaza tramas con MAC desconocida.

Soluciones:
- Usar interfaz cableada (Ethernet) para el bridge.
- Usar NAT para VMs cuando el host solo tiene WiFi.
- Algunos drivers WiFi soportan modo 4-address (WDS) que permite bridge, pero es raro.

---

## 8. Spanning Tree Protocol (STP)

Si un bridge tiene una topología con **loops** (caminos redundantes), las tramas circulan indefinidamente, saturando la red (broadcast storm). STP previene esto.

### El problema de los loops

```
Bridge A ──── Bridge B
    │              │
    └──── Bridge C ─┘
           │
         Loop: una trama broadcast puede circular A→B→C→A→B→C...
         indefinidamente, consumiendo todo el ancho de banda
```

### Cómo STP lo resuelve

STP desactiva puertos redundantes para eliminar loops, manteniendo un solo camino activo:

```
Bridge A ──── Bridge B
    │              │
    └──── Bridge C ─┤
                    X ← puerto bloqueado por STP
```

Si el camino activo falla, STP reactiva el puerto bloqueado como respaldo.

### STP en libvirt

Los bridges de libvirt tienen STP habilitado por defecto:

```xml
<bridge name='virbr0' stp='on' delay='0'/>
```

El `delay` controla cuánto espera STP antes de empezar a reenviar tramas (mientras aprende la topología). Para labs simples, `delay='0'` es correcto.

### ¿Importa STP para labs de VMs?

En un lab simple con un solo bridge y unas pocas VMs, los loops son imposibles y STP no hace nada útil. Pero si creas topologías complejas con múltiples bridges conectados entre sí (ej. simular una red empresarial), STP evita que un error de configuración tumbe toda la red virtual.

---

## 9. Tipos de bridge en Linux

### Linux bridge (kernel)

El bridge nativo del kernel de Linux. Es el más usado en virtualización con libvirt:

```bash
# Crear un bridge
sudo ip link add br0 type bridge

# Añadir una interfaz al bridge
sudo ip link set enp0s3 master br0

# Activar el bridge
sudo ip link set br0 up
```

Ventajas: integrado en el kernel, estable, compatible con todo.

### Open vSwitch (OVS)

Un switch virtual más avanzado, usado en entornos de cloud (OpenStack, oVirt):

```bash
# Crear un bridge OVS
sudo ovs-vsctl add-br ovs-br0

# Añadir una interfaz
sudo ovs-vsctl add-port ovs-br0 enp0s3
```

Ventajas: VLANs, QoS, SDN (Software-Defined Networking), OpenFlow, túneles VXLAN/GRE.
Desventajas: más complejo, requiere paquete adicional.

Para este curso usaremos Linux bridge — es más que suficiente para labs de virtualización.

### macvtap

Una alternativa al bridge que conecta VMs directamente a la interfaz física sin crear un bridge explícito:

```xml
<!-- En libvirt, usar macvtap en lugar de bridge -->
<interface type='direct' trustGuestRxFilters='yes'>
  <source dev='enp0s3' mode='bridge'/>
  <model type='virtio'/>
</interface>
```

Ventajas: más simple (no necesitas crear un bridge).
Desventajas: la VM no puede comunicarse con el host por esta interfaz (limitación de macvtap).

---

## 10. Escenarios: cuándo usar bridge y cuándo no

### Usar bridge cuando

| Escenario | Por qué bridge |
|-----------|---------------|
| VM que ofrece un servicio web accesible desde la LAN | Otros dispositivos necesitan alcanzar la VM |
| Simular una red "real" con VMs como si fueran PCs | Las VMs necesitan IPs en la misma red que el host |
| Lab de redes con routing entre subredes | Las VMs necesitan ser endpoints reales |
| VM con IP pública (hosting, servidor expuesto) | La VM debe ser directamente accesible |
| Cluster de VMs que necesitan multicast | NAT no reenvía multicast bien |

### Usar NAT cuando

| Escenario | Por qué NAT |
|-----------|------------|
| Labs de desarrollo general | Solo necesitas que la VM tenga internet |
| VMs temporales (testing, build) | No importa la IP ni la visibilidad |
| Host con WiFi (sin Ethernet) | Bridge con WiFi es problemático |
| Aislamiento deseado | No quieres que la VM sea visible en la LAN |
| Setup rápido sin configuración | NAT funciona out-of-the-box con libvirt |

### Usar red aislada cuando

| Escenario | Por qué aislada |
|-----------|----------------|
| Labs de seguridad/firewall | Las VMs no deben tocar la red real |
| Labs de almacenamiento (RAID, LVM) | La red no es relevante |
| Simulación de red interna sin internet | Por diseño |
| Entorno de malware analysis | Aislamiento total requerido |

---

## 11. El bridge en el contexto del curso

En este curso trabajarás con los tres tipos de red:

```
B02 C10 (este capítulo):
  → Entender los conceptos (teoría)
  → Crear un bridge manualmente (siguiente tópico)

B08 QEMU/KVM:
  → Usar la red NAT default para la mayoría de labs
  → Crear un bridge para labs donde las VMs necesitan ser accesibles

B09/B10:
  → Topologías multi-VM con redes aisladas y bridges
  → Router VM conectando subredes

B11 Docker:
  → Docker usa su propio bridge (docker0) — mismo concepto
```

El bridge que creas manualmente en el siguiente tópico (T02) es el mismo tipo de bridge que libvirt crea automáticamente (virbr0), pero conectado a la red física en lugar de aislado con NAT.

---

## 12. Errores comunes

### Error 1: Confundir bridge con NAT

```
"Mi VM tiene bridge, entonces tiene NAT"
→ FALSO. Bridge y NAT son mutuamente excluyentes.

Bridge: la VM está EN la red física (capa 2)
NAT:    la VM está DETRÁS del host (capa 3 con traducción)
```

virbr0 de libvirt es un bridge, sí, pero con NAT encima. Un bridge "puro" (br0 conectado a enp0s3) no tiene NAT — las VMs tienen IPs directamente en la LAN.

### Error 2: Crear un bridge y perder la conexión SSH al host

```bash
# Estás conectado por SSH al host (192.168.1.50 en enp0s3)
# Creas un bridge y añades enp0s3:
sudo ip link add br0 type bridge
sudo ip link set enp0s3 master br0
# → enp0s3 pierde su IP
# → tu sesión SSH SE CUELGA

# Solución: hacer TODOS los pasos en un solo comando o script:
sudo ip link add br0 type bridge && \
sudo ip link set enp0s3 master br0 && \
sudo ip addr del 192.168.1.50/24 dev enp0s3 && \
sudo ip addr add 192.168.1.50/24 dev br0 && \
sudo ip link set br0 up && \
sudo ip route add default via 192.168.1.1 dev br0
```

O mejor: configurar el bridge con NetworkManager o netplan, que maneja la transición automáticamente.

### Error 3: Intentar bridge con WiFi

```bash
sudo ip link set wlp2s0 master br0
# RTNETLINK answers: Operation not supported
# WiFi en modo cliente NO soporta bridge
```

WiFi no permite tramas con MAC de origen diferente a la de la interfaz. Las VMs tienen MACs propias (52:54:00:xx) que el access point rechaza.

### Error 4: No mover la IP al bridge

```bash
# Creas el bridge y añades enp0s3, pero no mueves la IP:
sudo ip link add br0 type bridge
sudo ip link set enp0s3 master br0
sudo ip link set br0 up

# enp0s3 ya no tiene IP (la perdió al ser puerto del bridge)
# br0 tampoco tiene IP (no se la asignaste)
# → el host pierde toda conectividad de red
```

Cuando enp0s3 se convierte en puerto del bridge, su IP debe moverse a br0.

### Error 5: Pensar que un bridge proporciona aislamiento

```
"Mis VMs están en un bridge, así que están aisladas"
→ FALSO. Un bridge CONECTA — no aísla.

Las VMs en un bridge son visibles para TODOS los dispositivos de la red.
Si quieres aislamiento, usa una red aislada de libvirt (sin bridge a la interfaz física).
```

---

## 13. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                   CONCEPTO DE BRIDGE CHEATSHEET                     ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  QUÉ ES                                                           ║
║  Bridge = switch virtual en software (capa 2, Ethernet)            ║
║  Conecta segmentos de red → misma red a nivel L2                   ║
║  Decide por MAC (no por IP) → examina tramas, no paquetes          ║
║  Aprende MACs observando el tráfico (MAC learning + aging)         ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  BRIDGE vs OTROS                                                   ║
║  Hub:     capa 1, reenvía TODO a todos (obsoleto)                  ║
║  Bridge:  capa 2, reenvía selectivamente por MAC                   ║
║  Switch:  = bridge moderno con muchos puertos                      ║
║  Router:  capa 3, conecta redes DIFERENTES por IP                  ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  EN VIRTUALIZACIÓN                                                 ║
║  Bridge puro:  VM tiene IP en la LAN real del host                 ║
║    VM (192.168.1.150) → vnet0 → br0 → enp0s3 → router             ║
║    VM visible desde toda la LAN                                    ║
║                                                                    ║
║  NAT (virbr0): VM tiene IP en red privada, NAT al salir            ║
║    VM (192.168.122.100) → vnet0 → virbr0 → MASQUERADE → enp0s3    ║
║    VM invisible desde la LAN                                       ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  COMPONENTES                                                       ║
║  br0        Bridge (switch virtual)                                 ║
║  enp0s3     Interfaz física (uplink del bridge)                     ║
║  vnetN      TAP device (cable virtual de la VM)                     ║
║  Tabla MAC  Mapeo MAC → puerto (aprendida automáticamente)          ║
║  STP        Previene loops en topologías redundantes                ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  IP DEL HOST CON BRIDGE                                            ║
║  ANTES:  enp0s3 tiene la IP                                        ║
║  DESPUÉS: enp0s3 sin IP (es puerto), br0 tiene la IP              ║
║  ¡Si no mueves la IP, el host pierde conectividad!                 ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CUÁNDO USAR                                                       ║
║  Bridge:  VM debe ser visible en la LAN → servicios, labs reales   ║
║  NAT:     VM solo necesita internet → desarrollo, testing          ║
║  Aislada: VM no debe tocar la red real → seguridad, RAID labs      ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  LIMITACIONES                                                      ║
║  WiFi NO soporta bridge (MAC filtering del AP)                     ║
║  Crear bridge por SSH → riesgo de perder conexión                  ║
║  Bridge no aísla — CONECTA (si quieres aislar, usa red aislada)    ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 14. Ejercicios

### Ejercicio 1: Identificar bridges en tu sistema

```bash
# Ver si tienes bridges
ip link show type bridge
bridge link show 2>/dev/null

# Ver virbr0 (si tienes libvirt)
ip addr show virbr0 2>/dev/null
bridge link show virbr0 2>/dev/null

# Ver docker0 (si tienes Docker)
ip addr show docker0 2>/dev/null
```

**Preguntas:**
1. ¿Cuántos bridges tienes? ¿Cuáles son?
2. ¿virbr0 tiene interfaces miembro (vnetN)? Si no, ¿por qué? (pista: ¿hay VMs corriendo?)
3. ¿virbr0 tiene IP? ¿En qué red está? ¿Es la red NAT de libvirt?
4. Si tienes Docker, ¿docker0 es un bridge? ¿Qué interfaces tiene conectadas?
5. ¿Tu interfaz física (enp0s3, ens33, etc.) es miembro de algún bridge, o funciona de forma independiente?

### Ejercicio 2: Topología y flujo de paquetes

Dibuja la topología de red de tu host (en papel o texto) incluyendo:
- Tu interfaz física y su IP.
- Bridges presentes (virbr0, docker0, br0).
- VMs o contenedores conectados (si los hay).
- El router de tu LAN.

Luego traza el camino de un paquete en estos escenarios:

1. **VM con NAT** hace `ping google.com`: ¿por qué interfaces pasa? ¿Dónde se hace NAT?
2. **VM con bridge** (hipotético) hace `ping google.com`: ¿por qué interfaces pasa? ¿Hay NAT?
3. **Laptop de un compañero** en tu LAN quiere acceder a un nginx en una VM NAT: ¿puede? ¿Qué necesita?
4. **Laptop de un compañero** en tu LAN quiere acceder a un nginx en una VM con bridge: ¿puede? ¿Es más simple?

### Ejercicio 3: Decisiones de diseño

Para cada escenario, indica qué tipo de red usarías (NAT, bridge, o aislada) y justifica:

1. Una VM de desarrollo donde solo compilas código y necesitas `apt install`.
2. Una VM con un servidor web que tu equipo necesita probar desde sus laptops.
3. Tres VMs que simulan un cluster de base de datos — necesitan comunicarse entre sí pero NO con internet.
4. Una VM donde pruebas malware en un entorno controlado.
5. Una VM que actúa como router entre dos redes virtuales.
6. Tu host solo tiene WiFi (sin Ethernet). Necesitas una VM con internet.

**Pregunta reflexiva:** En el escenario 5 (VM como router), ¿la VM router necesita estar en un bridge, en NAT, o en ambas? ¿Cuántas interfaces de red necesitaría?

---

> **Nota**: un bridge es conceptualmente simple — es un switch implementado en software. Pero su impacto en la arquitectura de red es significativo: transforma a las VMs de entidades ocultas detrás de NAT a ciudadanos de primera clase en la red. El precio es la complejidad de configuración (mover la IP del host al bridge, incompatibilidad con WiFi, riesgo de perder conectividad SSH). Para la mayoría de labs, NAT es suficiente. Usa bridge cuando necesites que las VMs sean accesibles desde fuera del host.
