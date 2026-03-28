# Tipos de Redes en libvirt

## Índice

1. [Redes virtuales: qué problema resuelven](#1-redes-virtuales-qué-problema-resuelven)
2. [Los modos de red en libvirt](#2-los-modos-de-red-en-libvirt)
3. [NAT (forward mode='nat')](#3-nat-forward-modenat)
4. [Aislada (sin forward / mode='none')](#4-aislada-sin-forward--modenone)
5. [Routed (forward mode='route')](#5-routed-forward-moderoute)
6. [Bridge (forward mode='bridge')](#6-bridge-forward-modebridge)
7. [Open (forward mode='open')](#7-open-forward-modeopen)
8. [Comparación completa de los 5 modos](#8-comparación-completa-de-los-5-modos)
9. [XML de cada tipo de red](#9-xml-de-cada-tipo-de-red)
10. [Múltiples redes: VMs con varias interfaces](#10-múltiples-redes-vms-con-varias-interfaces)
11. [Qué tipo de red usar en cada escenario del curso](#11-qué-tipo-de-red-usar-en-cada-escenario-del-curso)
12. [Errores comunes](#12-errores-comunes)
13. [Cheatsheet](#13-cheatsheet)
14. [Ejercicios](#14-ejercicios)

---

## 1. Redes virtuales: qué problema resuelven

Cuando creas VMs con QEMU/KVM, cada una necesita conectividad de red: comunicarse con otras VMs, con el host, o con internet. Pero no todas las VMs necesitan la misma conectividad:

- Una VM de desarrollo necesita internet para instalar paquetes.
- Una VM que simula un servidor de base de datos solo necesita hablar con la VM de la aplicación.
- Una VM donde analizas malware no debe tocar la red real bajo ningún concepto.

Libvirt resuelve esto con **redes virtuales**: redes definidas por XML que determinan exactamente cómo se conectan las VMs. Cada red virtual crea un bridge, opcionalmente incluye DHCP/DNS (via dnsmasq), y define si hay salida a internet y de qué tipo.

---

## 2. Los modos de red en libvirt

El elemento `<forward mode='...'>` en el XML de la red define el comportamiento:

| Modo | Elemento XML | Resumen |
|------|-------------|---------|
| **NAT** | `<forward mode='nat'/>` | VMs salen a internet via NAT (MASQUERADE) |
| **Aislada** | Sin `<forward>` | VMs solo hablan entre sí, sin salida |
| **Routed** | `<forward mode='route'/>` | VMs ruteadas sin NAT (requiere config en la LAN) |
| **Bridge** | `<forward mode='bridge'/>` | VMs conectadas a un bridge externo existente |
| **Open** | `<forward mode='open'/>` | Bridge sin reglas de firewall de libvirt |

Los dos primeros (NAT y aislada) son los más usados en labs. Bridge se usa cuando las VMs deben ser accesibles desde la LAN. Routed y open son para escenarios más específicos.

---

## 3. NAT (forward mode='nat')

El modo por defecto de libvirt. Las VMs obtienen IPs en una red privada y acceden a internet a través de NAT (MASQUERADE) en el host.

### Topología

```
┌─────────────────────────── HOST ────────────────────────────┐
│                                                              │
│  ┌── VM1 ──────┐   ┌── VM2 ──────┐                         │
│  │ 192.168.122  │   │ 192.168.122  │                         │
│  │      .100    │   │      .101    │                         │
│  └──────┬───────┘   └──────┬───────┘                         │
│         │ vnet0             │ vnet1                           │
│         └────────┬──────────┘                                │
│                  │                                            │
│  ┌───────────────┴───────────────┐                           │
│  │  virbr0 (192.168.122.1)       │                           │
│  │  bridge + DHCP + DNS + NAT    │                           │
│  └───────────────┬───────────────┘                           │
│                  │ MASQUERADE                                │
│                  │ (iptables/nftables)                       │
│  ┌───────────────┴───────────────┐                           │
│  │  enp0s3 (192.168.1.50)        │                           │
│  └───────────────┬───────────────┘                           │
└──────────────────┼───────────────────────────────────────────┘
                   │
              Router / Internet
```

### Conectividad

| Dirección | ¿Funciona? | Cómo |
|-----------|-----------|------|
| VM → Internet | Sí | NAT MASQUERADE via enp0s3 |
| VM → Host | Sí | Via virbr0 (192.168.122.1) |
| VM → VM (misma red) | Sí | Directo por el bridge |
| Host → VM | Sí | Via IP de la VM (192.168.122.x) |
| LAN → VM | No* | La VM no tiene IP en la LAN |
| Internet → VM | No* | Doble NAT lo impide |

*Posible con port forwarding (DNAT), pero no es directo.

### Qué crea libvirt automáticamente

Cuando activas la red NAT default:

1. **Bridge**: `virbr0`
2. **Proceso dnsmasq**: DHCP (192.168.122.2–254) + DNS forwarding
3. **Reglas iptables/nftables**: MASQUERADE para 192.168.122.0/24
4. **Reglas FORWARD**: permitir tráfico saliente de las VMs
5. **ip_forward**: se activa a 1 si no lo estaba

### Cuándo usar NAT

- Labs de desarrollo donde las VMs necesitan internet.
- Entornos donde la VM no necesita ser accesible desde fuera del host.
- La opción más simple — funciona sin configuración adicional.

---

## 4. Aislada (sin forward / mode='none')

Una red sin `<forward>` o con `<forward mode='none'/>` es completamente aislada: las VMs se comunican entre sí, pero no tienen salida a internet ni al host (a nivel de red — el host sí puede acceder si le asignas IP al bridge).

### Topología

```
┌─────────────────────────── HOST ────────────────────────────┐
│                                                              │
│  ┌── VM1 ──────┐   ┌── VM2 ──────┐   ┌── VM3 ──────┐      │
│  │ 10.0.1.100   │   │ 10.0.1.101   │   │ 10.0.1.102   │      │
│  └──────┬───────┘   └──────┬───────┘   └──────┬───────┘      │
│         └────────────┬─────┴──────────────────┘              │
│                      │                                        │
│  ┌───────────────────┴──────────────────┐                    │
│  │  virbr1 (10.0.1.1)                   │                    │
│  │  bridge + DHCP + DNS                  │                    │
│  │  SIN NAT — SIN FORWARD               │                    │
│  └──────────────────────────────────────┘                    │
│                                                              │
│  Sin conexión a enp0s3 ni a internet                         │
└──────────────────────────────────────────────────────────────┘
```

### Conectividad

| Dirección | ¿Funciona? |
|-----------|-----------|
| VM → Internet | No |
| VM → Host | Parcial* |
| VM → VM (misma red) | Sí |
| Host → VM | Parcial* |
| LAN → VM | No |

*El host tiene una IP en virbr1 (10.0.1.1), así que puede hacer ping a las VMs y viceversa. Pero no hay NAT ni forwarding — la comunicación se limita a esa red aislada. Libvirt añade reglas de firewall que restringen el tráfico host↔VM en redes aisladas, pero el host siempre puede acceder via `virsh console`.

### Qué crea libvirt

1. **Bridge**: `virbr1` (o el nombre que le des)
2. **Proceso dnsmasq**: DHCP + DNS (solo dentro de la red aislada)
3. **Sin reglas NAT**: no hay MASQUERADE
4. **Reglas FORWARD REJECT**: bloquea tráfico entre virbr1 y otras interfaces

### Cuándo usar aislada

| Escenario | Por qué |
|-----------|---------|
| Labs de almacenamiento (RAID, LVM) | No necesitan red externa |
| Labs de firewall/seguridad | VMs no deben escapar |
| Simulación de red interna | Red "corporativa" sin internet |
| Análisis de malware | Aislamiento total |
| Red backend entre VMs | App→DB sin exponer DB a nada más |

---

## 5. Routed (forward mode='route')

Modo menos conocido pero útil en ciertos escenarios. Las VMs tienen sus propias IPs (en una subred diferente a la LAN), y el host actúa como **router** (sin NAT) entre la red de VMs y la LAN.

### Topología

```
┌─────────────────────────── HOST ────────────────────────────┐
│                                                              │
│  ┌── VM1 ──────┐   ┌── VM2 ──────┐                         │
│  │ 10.10.0.100  │   │ 10.10.0.101  │                         │
│  └──────┬───────┘   └──────┬───────┘                         │
│         └────────┬──────────┘                                │
│                  │                                            │
│  ┌───────────────┴───────────────┐                           │
│  │  vibrN (10.10.0.1)            │                           │
│  │  bridge + routing (sin NAT)   │                           │
│  └───────────────┬───────────────┘                           │
│                  │ routing (ip_forward)                      │
│                  │ SIN MASQUERADE                            │
│  ┌───────────────┴───────────────┐                           │
│  │  enp0s3 (192.168.1.50)        │                           │
│  └───────────────┬───────────────┘                           │
└──────────────────┼───────────────────────────────────────────┘
                   │
              Router (192.168.1.1)
```

### Diferencia clave con NAT

En NAT, la IP de la VM se **reemplaza** por la del host (MASQUERADE). En routed, la IP original de la VM **se mantiene**. Pero para que esto funcione, el router de la LAN necesita una **ruta estática** hacia la red de VMs via el host:

```bash
# En el router de la LAN (o en cada máquina de la LAN):
ip route add 10.10.0.0/24 via 192.168.1.50
#                              ──────────────
#                              IP del host
```

Sin esta ruta, los dispositivos de la LAN no saben cómo llegar a 10.10.0.x y los paquetes de vuelta se pierden.

### Cuándo usar routed

- Entornos donde necesitas que las VMs sean accesibles con sus IPs reales (sin NAT).
- Cuando tienes control del router para añadir rutas estáticas.
- Labs de routing donde quieres practicar configuración de rutas.

En la mayoría de labs domésticos, NAT o bridge son más prácticos.

---

## 6. Bridge (forward mode='bridge')

En modo bridge, libvirt no crea un bridge propio — se conecta a un **bridge externo que ya existe en el host** (ej. `br0` creado con NetworkManager). Libvirt no gestiona DHCP ni NAT; las VMs se conectan directamente a la red física.

### Topología

```
┌─────────────────────────── HOST ────────────────────────────┐
│                                                              │
│  ┌── VM1 ──────┐   ┌── VM2 ──────┐                         │
│  │ 192.168.1    │   │ 192.168.1    │   (misma red que       │
│  │      .150    │   │      .151    │    la LAN)              │
│  └──────┬───────┘   └──────┬───────┘                         │
│         │ vnet0             │ vnet1                           │
│         └────────┬──────────┘                                │
│                  │                                            │
│  ┌───────────────┴───────────────┐                           │
│  │  br0 (192.168.1.50)           │  ← bridge externo,       │
│  │  (creado con nmcli/ip link)   │    ya existía             │
│  │  enp0s3 como puerto           │                           │
│  └───────────────┬───────────────┘                           │
└──────────────────┼───────────────────────────────────────────┘
                   │
              Router (192.168.1.1)
              DHCP asigna .150, .151 a las VMs
```

### Qué hace libvirt en modo bridge

Casi nada. Libvirt solo:
1. Crea el TAP device (vnetN) para cada VM.
2. Lo conecta al bridge externo especificado.

No crea bridge, no lanza dnsmasq, no configura NAT ni firewall. Todo depende de la configuración preexistente del bridge y de la LAN.

### Cuándo usar bridge

- VMs que ofrecen servicios accesibles desde la LAN (web servers, DBs).
- Entornos que simulan una red "real" con VMs como máquinas físicas.
- Cuando necesitas que otros dispositivos de la red alcancen las VMs directamente.

---

## 7. Open (forward mode='open')

El modo open es como NAT sin las reglas de firewall automáticas de libvirt. Libvirt crea el bridge pero **no añade ninguna regla de iptables/nftables**. Tú controlas completamente el firewall.

### Cuándo usar open

- Cuando usas tu propio firewall (nftables personalizado) y las reglas de libvirt interfieren.
- Entornos donde necesitas control total del tráfico entre la red virtual y el exterior.
- Cuando las reglas automáticas de libvirt causan conflictos con otras configuraciones.

```xml
<network>
  <name>custom-firewall</name>
  <forward mode='open'/>
  <bridge name='virbr5' stp='on' delay='0'/>
  <ip address='10.50.0.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.50.0.2' end='10.50.0.254'/>
    </dhcp>
  </ip>
</network>
```

Libvirt crea el bridge y dnsmasq, pero no toca iptables. Si quieres que las VMs tengan internet, debes añadir tus propias reglas MASQUERADE.

---

## 8. Comparación completa de los 5 modos

### Tabla de conectividad

```
                    VM→       VM→      Host→    LAN→     VM↔VM
                    Internet  Host     VM       VM       (misma red)
─────────────────── ───────── ──────── ──────── ──────── ──────────
NAT                 ✅        ✅       ✅*      ❌       ✅
Aislada             ❌        Parcial  Parcial  ❌       ✅
Routed              ✅**      ✅       ✅       ✅**     ✅
Bridge              ✅        ✅       ✅       ✅       ✅
Open                Depende   Depende  Depende  Depende  ✅

*  Host accede a la VM por su IP privada (192.168.122.x)
** Requiere rutas estáticas en el router de la LAN
```

### Tabla de componentes

| Componente | NAT | Aislada | Routed | Bridge | Open |
|-----------|-----|---------|--------|--------|------|
| Bridge creado por libvirt | Sí | Sí | Sí | No (externo) | Sí |
| dnsmasq (DHCP+DNS) | Sí | Sí | Sí | No | Sí |
| Reglas de firewall | Sí (MASQUERADE) | Sí (REJECT forward) | Sí (ACCEPT forward) | No | No |
| ip_forward necesario | Sí | No | Sí | No | Depende |
| Interfaz física en bridge | No | No | No | Sí | No |

### Diagrama de decisión

```
¿La VM necesita internet?
├── NO → ¿Las VMs necesitan comunicarse entre sí?
│         ├── SÍ → AISLADA
│         └── NO → (¿para qué necesitas red?)
│
└── SÍ → ¿La VM debe ser accesible desde la LAN?
          ├── NO → NAT (el default, lo más simple)
          │
          └── SÍ → ¿Tienes Ethernet (no WiFi)?
                    ├── SÍ → BRIDGE
                    └── NO → NAT + port forwarding
                              (o bridge macvtap si lo soporta)
```

---

## 9. XML de cada tipo de red

### NAT (con todas las opciones)

```xml
<network>
  <name>lab-nat</name>
  <forward mode='nat'>
    <nat>
      <port start='1024' end='65535'/>
    </nat>
  </forward>
  <bridge name='virbr0' stp='on' delay='0'/>
  <mac address='52:54:00:aa:bb:cc'/>
  <domain name='lab.nat' localOnly='yes'/>
  <dns>
    <host ip='192.168.122.10'>
      <hostname>web.lab.nat</hostname>
    </host>
  </dns>
  <ip address='192.168.122.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='192.168.122.2' end='192.168.122.254'/>
      <host mac='52:54:00:11:22:33' name='web-server' ip='192.168.122.10'/>
    </dhcp>
  </ip>
</network>
```

| Elemento | Función |
|----------|---------|
| `<forward mode='nat'>` | Habilitar NAT |
| `<port start/end>` | Rango de puertos para traducción NAT |
| `<bridge name>` | Nombre del bridge (virbr0, vibrN) |
| `<mac address>` | MAC del bridge (generada automáticamente si se omite) |
| `<domain name localOnly>` | Dominio DNS local (solo dentro de la red) |
| `<dns><host>` | Registros DNS personalizados |
| `<dhcp><range>` | Pool de IPs dinámicas |
| `<dhcp><host>` | Reserva DHCP (IP fija por MAC) |

### Aislada (mínima)

```xml
<network>
  <name>lab-isolated</name>
  <bridge name='virbr1' stp='on' delay='0'/>
  <ip address='10.0.1.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.0.1.2' end='10.0.1.254'/>
    </dhcp>
  </ip>
</network>
```

Sin `<forward>`: la red es aislada por defecto.

### Aislada (sin DHCP — IPs estáticas)

```xml
<network>
  <name>static-isolated</name>
  <bridge name='virbr2' stp='on' delay='0'/>
  <ip address='10.0.2.1' netmask='255.255.255.0'/>
</network>
```

Sin `<dhcp>`: las VMs necesitan IP estática configurada manualmente.

### Routed

```xml
<network>
  <name>lab-routed</name>
  <forward mode='route' dev='enp0s3'/>
  <bridge name='virbr3' stp='on' delay='0'/>
  <ip address='10.10.0.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.10.0.2' end='10.10.0.254'/>
    </dhcp>
  </ip>
</network>
```

`dev='enp0s3'`: especifica por qué interfaz sale el tráfico ruteado.

### Bridge (externo)

```xml
<network>
  <name>host-bridge</name>
  <forward mode='bridge'/>
  <bridge name='br0'/>
</network>
```

El XML más simple: solo apunta al bridge externo. No tiene `<ip>` ni `<dhcp>` porque eso lo gestiona la red real.

### Open

```xml
<network>
  <name>lab-open</name>
  <forward mode='open'/>
  <bridge name='virbr5' stp='on' delay='0'/>
  <ip address='10.50.0.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.50.0.2' end='10.50.0.254'/>
    </dhcp>
  </ip>
</network>
```

---

## 10. Múltiples redes: VMs con varias interfaces

Una VM puede tener **múltiples interfaces de red**, cada una conectada a una red diferente. Esto es esencial para simular topologías complejas.

### Caso: VM router entre red NAT y red aislada

```xml
<!-- VM con dos interfaces -->
<interface type='network'>
  <source network='default'/>          <!-- NAT: acceso a internet -->
  <model type='virtio'/>
</interface>
<interface type='network'>
  <source network='lab-isolated'/>     <!-- Aislada: red interna -->
  <model type='virtio'/>
</interface>
```

Dentro de la VM:
```bash
ip addr
# eth0: 192.168.122.100/24   ← red NAT (internet)
# eth1: 10.0.1.100/24        ← red aislada (interna)
```

### Topología multi-red

```
Internet
    │
┌───┴───── Red NAT (default) ──────────────────────┐
│   192.168.122.0/24                                │
│                                                    │
│   ┌── Router-VM ──┐                               │
│   │ eth0: .122.100 │──── virbr0 ──── (NAT) ──── Internet
│   │ eth1: .1.100   │                               │
│   └───────┬────────┘                               │
└───────────┼────────────────────────────────────────┘
            │
┌───────────┴── Red aislada (lab-isolated) ─────────┐
│   10.0.1.0/24                                      │
│                                                    │
│   ┌── Web-VM ─────┐   ┌── DB-VM ──────┐          │
│   │ eth0: .1.101   │   │ eth0: .1.102   │          │
│   └────────────────┘   └────────────────┘          │
│                                                    │
│   Estas VMs NO tienen internet directamente.       │
│   Si Router-VM habilita ip_forward + NAT,          │
│   pueden salir a internet a través de Router-VM.   │
└────────────────────────────────────────────────────┘
```

Este patrón es fundamental para los bloques B09/B10 del curso.

### Crear una VM con múltiples redes (virt-install)

```bash
virt-install \
    --name router-vm \
    --network network=default \
    --network network=lab-isolated \
    --disk size=10 \
    --os-variant fedora39 \
    ...
```

---

## 11. Qué tipo de red usar en cada escenario del curso

| Bloque | Escenario | Red recomendada | Justificación |
|--------|-----------|----------------|---------------|
| B08 C00 | Setup inicial QEMU/KVM | NAT (default) | Las VMs necesitan internet para instalar paquetes |
| B08 | Labs de almacenamiento (RAID, LVM) | Aislada | La red no es relevante, solo discos |
| B09 | Topologías multi-VM | NAT + Aislada | VM router con dos interfaces |
| B09 | Simular una red corporativa | Múltiples aisladas | Cada subred es una red aislada separada |
| B10 | Servidor DNS/DHCP | NAT + Aislada | VM servidor con internet + red interna |
| B10 | Cluster de servicios | Aislada | Comunicación inter-VM sin internet |
| B11 | Labs de firewall | Aislada | VMs no deben escapar a la red real |
| B11 | Docker | NAT (default de Docker) | docker0 es un bridge NAT, mismo concepto |
| General | VM de desarrollo | NAT | Internet para apt/dnf, sin exposición |
| General | VM con web server accesible | Bridge | Visible desde la LAN |

---

## 12. Errores comunes

### Error 1: Confundir "aislada" con "sin red"

```bash
# "La red aislada no tiene red, así que no necesito configurar nada"
# → INCORRECTO. La red aislada SÍ tiene red — las VMs se comunican entre sí.
#   Solo no tiene salida a internet.

# Una VM en red aislada con DHCP obtiene:
# IP: 10.0.1.100
# Gateway: 10.0.1.1 (pero no lleva a internet)
# DNS: 10.0.1.1 (solo resuelve hostnames de VMs locales)
```

### Error 2: Crear múltiples redes NAT con el mismo rango

```bash
virsh net-dumpxml default
# 192.168.122.0/24

virsh net-dumpxml my-nat
# 192.168.122.0/24    ← MISMO RANGO → CONFLICTO

# Resultado: routing ambiguo, una de las dos redes no funciona.
# Solución: cada red NAT necesita un rango diferente.
```

### Error 3: Usar bridge sin tener un bridge externo creado

```xml
<!-- Defines una red bridge: -->
<network>
  <name>my-bridge</name>
  <forward mode='bridge'/>
  <bridge name='br0'/>
</network>
```

```bash
virsh net-start my-bridge
# error: failed to start network 'my-bridge'
# Network interface 'br0' not found
```

El modo bridge requiere que `br0` **ya exista** en el host. Libvirt no lo crea.

### Error 4: Esperar DHCP en modo bridge

```bash
# La VM está en modo bridge pero no obtiene IP por DHCP

# ¿El DHCP de dónde vendría?
# En modo bridge, libvirt NO lanza dnsmasq.
# El DHCP debe venir del router real de la LAN.
# Si el router no tiene DHCP o no llega al bridge, la VM no obtiene IP.

# Diagnóstico dentro de la VM:
sudo dhclient -v eth0
# Si dice "No DHCPOFFERS" → el DHCP del router no llega
```

### Error 5: No entender que NAT y aislada tienen DHCP propio

```bash
# "Mi VM en red aislada no puede obtener IP por DHCP"
# → Sí puede. libvirt lanza dnsmasq para redes aisladas y NAT.
# → Verificar: virsh net-dhcp-leases lab-isolated

# "Mi VM en modo bridge no obtiene IP"
# → En bridge, NO hay dnsmasq de libvirt. El DHCP viene del router real.
```

---

## 13. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                 TIPOS DE REDES EN LIBVIRT CHEATSHEET                ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  MODOS DE RED                                                      ║
║  NAT       <forward mode='nat'/>     Internet via MASQUERADE       ║
║  Aislada   (sin <forward>)           Solo VM↔VM, sin internet      ║
║  Routed    <forward mode='route'/>   Routing sin NAT               ║
║  Bridge    <forward mode='bridge'/>  Bridge externo existente      ║
║  Open      <forward mode='open'/>    Sin reglas firewall libvirt   ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CONECTIVIDAD                                                      ║
║              VM→Net  VM→Host  Host→VM  LAN→VM  VM↔VM              ║
║  NAT          ✅      ✅       ✅*      ❌       ✅                ║
║  Aislada      ❌      ~        ~        ❌       ✅                ║
║  Routed       ✅†     ✅       ✅       ✅†      ✅                ║
║  Bridge       ✅      ✅       ✅       ✅       ✅                ║
║  * Via IP privada    † Requiere rutas estáticas                    ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  QUÉ GESTIONA LIBVIRT EN CADA MODO                                ║
║              Bridge  dnsmasq  Firewall  NAT                        ║
║  NAT          Crea    Sí       Sí        Sí                        ║
║  Aislada      Crea    Sí       Sí        No                        ║
║  Routed       Crea    Sí       Sí        No                        ║
║  Bridge       No      No       No        No                        ║
║  Open         Crea    Sí       No        No                        ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  DECISIÓN RÁPIDA                                                   ║
║  ¿Solo necesita internet?              → NAT                      ║
║  ¿Solo VM↔VM sin internet?             → Aislada                  ║
║  ¿Accesible desde la LAN?              → Bridge                   ║
║  ¿Router entre redes?                  → VM con 2+ interfaces     ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  MÚLTIPLES INTERFACES                                              ║
║  virt-install --network network=default --network network=lab      ║
║  virsh edit: múltiples <interface>                                 ║
║  Dentro de la VM: eth0 = primera red, eth1 = segunda red           ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  RANGOS COMUNES                                                    ║
║  NAT default:    192.168.122.0/24                                  ║
║  NAT adicional:  192.168.123.0/24, 192.168.124.0/24, ...          ║
║  Aislada:        10.0.1.0/24, 10.0.2.0/24, ...                   ║
║  Bridge:         (IP de la LAN real del host)                      ║
║  ¡Cada red necesita un rango ÚNICO!                                ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 14. Ejercicios

### Ejercicio 1: Inventario de redes en tu sistema

```bash
virsh net-list --all
virsh net-info default
virsh net-dumpxml default
```

**Preguntas:**
1. ¿Cuántas redes virtuales tienes definidas? ¿Cuántas están activas?
2. ¿La red default tiene `<forward mode='nat'/>`? ¿Tiene DHCP? ¿En qué rango?
3. ¿Qué bridge usa la red default? ¿Es virbr0?
4. Si quisieras crear una segunda red NAT, ¿qué rango de IPs usarías para evitar conflictos?

### Ejercicio 2: Diseño de redes para un lab multi-VM

Necesitas montar un lab con esta topología:

```
Internet
    │
    ├── VM "gateway" (2 interfaces: NAT + aislada)
    │       → Tiene internet
    │       → Conecta las VMs internas a internet (si habilitas forwarding)
    │
    ├── VM "web-server" (1 interfaz: aislada)
    │       → Solo accesible desde la red interna
    │
    └── VM "db-server" (1 interfaz: aislada)
            → Solo accesible desde la red interna
```

1. ¿Cuántas redes virtuales necesitas definir? ¿De qué tipo cada una?
2. Escribe el XML de la red aislada con rango 10.0.10.0/24 y nombre "lab-internal".
3. ¿Cuántas interfaces de red tiene la VM "gateway"? ¿A qué redes se conecta?
4. Escribe el comando `virt-install` (solo las flags `--network`) para la VM "gateway".
5. Si la VM "web-server" necesita instalar paquetes con `dnf install`, ¿puede? ¿Qué necesitarías configurar en la VM "gateway" para habilitarlo?

### Ejercicio 3: Comparación práctica

Para cada escenario, indica qué modo de red usarías y escribe el fragmento XML `<forward>` correspondiente:

1. Un lab donde 5 VMs ejecutan un cluster Kubernetes. Necesitan internet para descargar imágenes de contenedores, pero no necesitan ser accesibles desde la LAN.
2. Dos VMs que simulan un cliente y un servidor en una red corporativa aislada. No deben tener internet.
3. Una VM que actúa como servidor web. Tus compañeros de equipo necesitan accederla desde sus laptops en la misma LAN.
4. Un lab de firewall donde una VM maliciosa intenta atacar a otra VM. Bajo ninguna circunstancia el tráfico debe llegar a la red real.
5. Una VM que necesita internet y donde tú controlas las reglas de firewall con nftables (las reglas automáticas de libvirt te molestan).

**Pregunta reflexiva:** Imagina que necesitas combinar los escenarios 1 y 4: un cluster Kubernetes aislado pero con internet. ¿Cómo lo harías? (pista: VM router con dos interfaces).

---

> **Nota**: los tipos de red en libvirt son la herramienta que define la topología de tu lab. NAT y aislada cubren el 90% de los casos. La clave es entender que cada tipo genera diferentes componentes (bridge, dnsmasq, reglas de firewall) y que la elección correcta depende de una pregunta simple: ¿quién necesita hablar con quién? Una vez respondes eso, el diagrama de decisión te lleva directamente al modo correcto.
