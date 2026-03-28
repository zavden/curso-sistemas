# T03 — systemd-networkd: Archivos .network y Cuándo Usarlo sobre NetworkManager

## Índice

1. [¿Qué es systemd-networkd?](#1-qué-es-systemd-networkd)
2. [Cuándo usar systemd-networkd vs NetworkManager](#2-cuándo-usar-systemd-networkd-vs-networkmanager)
3. [Arquitectura y componentes](#3-arquitectura-y-componentes)
4. [Archivos .network — Configuración de red](#4-archivos-network--configuración-de-red)
5. [Archivos .netdev — Interfaces virtuales](#5-archivos-netdev--interfaces-virtuales)
6. [Archivos .link — Propiedades de enlace](#6-archivos-link--propiedades-de-enlace)
7. [Sección [Match] — Seleccionar interfaces](#7-sección-match--seleccionar-interfaces)
8. [DHCP con systemd-networkd](#8-dhcp-con-systemd-networkd)
9. [IP estática con systemd-networkd](#9-ip-estática-con-systemd-networkd)
10. [Rutas, DNS y dominios](#10-rutas-dns-y-dominios)
11. [networkctl — Control y diagnóstico](#11-networkctl--control-y-diagnóstico)
12. [Integración con systemd-resolved](#12-integración-con-systemd-resolved)
13. [Configuraciones avanzadas](#13-configuraciones-avanzadas)
14. [Errores comunes](#14-errores-comunes)
15. [Cheatsheet](#15-cheatsheet)
16. [Ejercicios](#16-ejercicios)

---

## 1. ¿Qué es systemd-networkd?

systemd-networkd es un daemon de gestión de red que forma parte de systemd. Es **declarativo**: defines el estado deseado en archivos de configuración y el daemon lo aplica. No tiene GUI, no gestiona WiFi directamente (necesita wpa_supplicant), y no tiene plugins para VPN. A cambio, es extremadamente ligero, predecible y fácil de automatizar.

```
Filosofía de systemd-networkd:

"Describe el estado final que quieres. Yo lo aplico."

Archivo .network:                    Estado resultante:
┌──────────────────────┐             ┌──────────────────────┐
│ [Match]              │             │ eth0:                │
│ Name=eth0            │             │   IP: 192.168.1.50/24│
│                      │  ────────►  │   GW: 192.168.1.1   │
│ [Network]            │             │   DNS: 1.1.1.1       │
│ Address=192.168.1.50 │             │   Estado: routable   │
│ Gateway=192.168.1.1  │             └──────────────────────┘
│ DNS=1.1.1.1          │
└──────────────────────┘
```

systemd-networkd se diseñó para escenarios donde la configuración de red es estable y conocida de antemano: servidores, contenedores, máquinas virtuales, sistemas embebidos.

---

## 2. Cuándo usar systemd-networkd vs NetworkManager

Esta es una de las decisiones más frecuentes al administrar Linux. No hay una respuesta universal, pero hay criterios claros:

```
¿Qué necesitas?
│
├── WiFi, VPN, interfaces que van y vienen
│   └──► NetworkManager
│
├── Servidor con red estable, pocos cambios
│   └──► systemd-networkd
│
├── Contenedores / VMs / infraestructura automatizada
│   └──► systemd-networkd
│
├── Desktop con docking station, redes móviles
│   └──► NetworkManager
│
├── Appliance / sistema embebido
│   └──► systemd-networkd
│
└── "No sé, solo quiero que funcione"
    └──► Lo que tu distro traiga por defecto
```

| Criterio | NetworkManager | systemd-networkd |
|----------|---------------|------------------|
| Huella de memoria | ~15-30 MB | ~2-5 MB |
| WiFi integrado | Sí | No (necesita wpa_supplicant) |
| VPN | Plugins nativos | No |
| GUI | nm-applet, GNOME Settings | No |
| TUI | nmtui | No |
| CLI | nmcli | networkctl |
| Config dinámica | Excelente (cambio de perfil, hotplug) | Limitada (reconfigurar) |
| Automatización | nmcli funciona, pero es imperativo | Archivos declarativos, fácil de gestionar con Ansible/scripts |
| Dependencias | D-Bus, polkit, muchas libs | Solo systemd |
| Gestión de bridges/bonds/VLANs | Sí | Sí (archivos .netdev) |
| Distribución por defecto | Fedora, RHEL, Ubuntu Desktop | Ubuntu Server, containers |

### Pueden coexistir

Ambos pueden correr en el mismo sistema si gestionan interfaces **distintas**:

```
Ejemplo: laptop con docking station

systemd-networkd gestiona:       NetworkManager gestiona:
├── eth0 (puerto LAN del dock)   ├── wlan0 (WiFi)
│   Config estable, IP fija      │   Cambia entre redes
└── eth1 (red de lab)            ├── VPN corporativa
    Config estable, VLAN         └── tun0 (WireGuard)
```

---

## 3. Arquitectura y componentes

systemd-networkd trabaja con tres tipos de archivos de configuración:

```
/etc/systemd/network/
├── 10-eth0.link         ← .link: propiedades de hardware (MAC, MTU, nombre)
├── 20-bridge.netdev     ← .netdev: crear interfaces virtuales
├── 25-eth0.network      ← .network: configuración IP, DHCP, rutas, DNS
└── 30-bridge.network    ← .network: configuración del bridge
```

```
Relación entre los tres tipos:

.link                  .netdev                .network
(propiedades L1/L2)    (crear interfaz)       (configurar L3)
│                      │                      │
│ Renombrar eth0→lan0  │ Crear br0 (bridge)   │ Asignar IP a lan0
│ Cambiar MTU          │ Crear bond0           │ DHCP en br0
│ Fijar MAC            │ Crear vlan100         │ Rutas, DNS
│                      │                      │
▼                      ▼                      ▼
Procesado por          Procesado por          Procesado por
systemd-udevd          systemd-networkd       systemd-networkd
(muy temprano)         (al arrancar)          (al arrancar)
```

**Orden de carga**: los archivos se procesan en orden alfanumérico. Por eso se usa un prefijo numérico (10-, 20-, 25-) para controlar el orden. Los números más bajos se procesan primero.

**Directorios de búsqueda** (en orden de prioridad):

| Directorio | Uso |
|------------|-----|
| `/etc/systemd/network/` | Configuración del administrador (mayor prioridad) |
| `/run/systemd/network/` | Configuración en runtime (generada dinámicamente) |
| `/usr/lib/systemd/network/` | Defaults del sistema/paquetes (menor prioridad) |

Un archivo en `/etc/` con el mismo nombre que uno en `/usr/lib/` lo sobrescribe completamente.

---

## 4. Archivos .network — Configuración de red

Los archivos `.network` son el corazón de systemd-networkd. Definen cómo configurar una interfaz que ya existe.

### Estructura básica

```ini
# /etc/systemd/network/25-eth0.network

[Match]
Name=eth0

[Network]
DHCP=yes
```

Eso es todo lo necesario para configurar eth0 con DHCP. La simplicidad es una de las fortalezas de systemd-networkd.

### Secciones disponibles

```ini
[Match]          # Seleccionar a qué interfaces aplica este archivo
[Link]           # Propiedades de enlace (MTU, etc.) — dentro de .network
[Network]        # Configuración general (DHCP, DNS, direcciones)
[Address]        # Dirección IP (puede haber múltiples secciones)
[Route]          # Ruta estática (puede haber múltiples secciones)
[DHCPv4]         # Opciones del cliente DHCPv4
[DHCPv6]         # Opciones del cliente DHCPv6
[IPv6AcceptRA]   # Opciones de Router Advertisement
[Bridge]         # Si esta interfaz es miembro de un bridge
[BridgeVLAN]     # VLANs en bridge
```

### Ejemplo completo anotado

```ini
# /etc/systemd/network/25-main.network

[Match]
Name=eth0                          # Aplica a eth0

[Link]
MTUBytes=1500                      # MTU (también configurable en .link)

[Network]
Description=Main ethernet          # Descripción (informativa)
DHCP=yes                           # DHCP para IPv4 e IPv6
DNS=1.1.1.1                        # DNS adicional (además del DHCP)
Domains=empresa.local              # Dominio de búsqueda
NTP=pool.ntp.org                   # Servidor NTP
LLDP=yes                           # Escuchar LLDP (Link Layer Discovery)
EmitLLDP=nearest-bridge            # Emitir LLDP

[DHCPv4]
UseDNS=yes                         # Usar DNS del DHCP
UseRoutes=yes                      # Usar rutas del DHCP
UseHostname=no                     # No cambiar hostname por DHCP
SendHostname=yes                   # Enviar nuestro hostname
RouteMetric=100                    # Prioridad de rutas DHCP
```

---

## 5. Archivos .netdev — Interfaces virtuales

Los archivos `.netdev` crean interfaces virtuales. Se procesan antes que los `.network`.

### Bridge

```ini
# /etc/systemd/network/20-br0.netdev
[NetDev]
Name=br0
Kind=bridge

[Bridge]
STP=yes                    # Spanning Tree Protocol
ForwardDelaySec=4
```

```ini
# /etc/systemd/network/25-br0.network
# Configurar IP del bridge
[Match]
Name=br0

[Network]
Address=192.168.1.1/24
```

```ini
# /etc/systemd/network/30-eth0-bridge.network
# Agregar eth0 al bridge
[Match]
Name=eth0

[Network]
Bridge=br0
# eth0 no tiene IP propia, es miembro del bridge
```

### VLAN

```ini
# /etc/systemd/network/20-vlan100.netdev
[NetDev]
Name=vlan100
Kind=vlan

[VLAN]
Id=100
```

```ini
# /etc/systemd/network/25-eth0.network
# Interfaz padre de la VLAN
[Match]
Name=eth0

[Network]
DHCP=yes
VLAN=vlan100               # Asociar la VLAN a esta interfaz
```

```ini
# /etc/systemd/network/30-vlan100.network
# Configurar la VLAN
[Match]
Name=vlan100

[Network]
Address=10.100.0.5/24
Gateway=10.100.0.1
```

### Bond

```ini
# /etc/systemd/network/20-bond0.netdev
[NetDev]
Name=bond0
Kind=bond

[Bond]
Mode=802.3ad                # LACP
MIIMonitorSec=100ms
LACPTransmitRate=fast
```

```ini
# /etc/systemd/network/25-bond0-members.network
# Interfaces miembros del bond
[Match]
Name=eth0 eth1

[Network]
Bond=bond0
```

```ini
# /etc/systemd/network/30-bond0.network
# Configurar IP del bond
[Match]
Name=bond0

[Network]
DHCP=yes
```

### Interfaz dummy (testing)

```ini
# /etc/systemd/network/20-dummy0.netdev
[NetDev]
Name=dummy0
Kind=dummy
```

```ini
# /etc/systemd/network/25-dummy0.network
[Match]
Name=dummy0

[Network]
Address=10.99.0.1/32
```

---

## 6. Archivos .link — Propiedades de enlace

Los archivos `.link` configuran propiedades de bajo nivel de las interfaces. Son procesados por **systemd-udevd** (no por systemd-networkd), muy temprano en el arranque, incluso antes de que la interfaz tenga nombre.

```ini
# /etc/systemd/network/10-eth0.link

[Match]
MACAddress=aa:bb:cc:11:22:33      # Coincidir por MAC (más fiable)
# O: Path=pci-0000:02:00.0        # Por ruta PCI

[Link]
Name=lan0                          # Renombrar la interfaz
MTUBytes=9000                      # Jumbo frames
WakeOnLan=magic                    # Wake-on-LAN
MACAddressPolicy=persistent        # Mantener MAC original
# MACAddressPolicy=random          # MAC aleatoria (privacidad)
```

> **Cuidado**: cambiar el nombre de una interfaz en un `.link` requiere que los archivos `.network` usen el nombre nuevo, no el original.

### Nombres de interfaz predecibles

systemd ya proporciona un archivo `.link` por defecto (`/usr/lib/systemd/network/99-default.link`) que activa los **nombres predecibles** (Predictable Network Interface Names):

```
Nombres legacy:       Nombres predecibles:
eth0, eth1, ...       enp0s3, enp0s8, ...
wlan0, wlan1, ...     wlp2s0, ...

Formato: en = ethernet, wl = wireless
         p0 = PCI bus 0
         s3 = slot 3
```

Los nombres predecibles evitan el problema de que el kernel asigne eth0/eth1 en orden diferente tras un reinicio o cambio de hardware.

---

## 7. Sección [Match] — Seleccionar interfaces

La sección `[Match]` determina a qué interfaces se aplica un archivo de configuración. Si todos los criterios coinciden, el archivo se aplica. Si no hay sección `[Match]`, el archivo aplica a **todas** las interfaces.

```ini
# Por nombre exacto
[Match]
Name=eth0

# Por patrón (glob)
[Match]
Name=en*               # Todas las interfaces ethernet (nombres predecibles)

# Múltiples nombres
[Match]
Name=eth0 eth1 eth2    # Cualquiera de estos (OR)

# Por MAC
[Match]
MACAddress=aa:bb:cc:11:22:33

# Por tipo
[Match]
Type=ether             # Solo interfaces ethernet

# Por driver
[Match]
Driver=virtio_net      # Solo interfaces virtio (VMs)

# Por ruta PCI
[Match]
Path=pci-0000:03:00.0

# Combinaciones (AND — todos deben coincidir)
[Match]
Name=eth*
Type=ether
Driver=e1000

# Por virtualización
[Match]
Virtualization=yes     # Solo dentro de VMs/contenedores
# Virtualization=no    # Solo en hardware físico
# Virtualization=container  # Solo contenedores
# Virtualization=vm         # Solo VMs

# Por hostname
[Match]
Host=webserver-*       # Solo en hosts cuyo nombre coincida

# Por arquitectura
[Match]
Architecture=x86-64
```

### Orden de prioridad

Cuando múltiples archivos `.network` coinciden con una interfaz, **solo se aplica el primero** (en orden alfanumérico). Por eso los prefijos numéricos son importantes:

```
/etc/systemd/network/
├── 10-loopback.network      ← Aplica a lo
├── 20-eth0-special.network  ← Aplica a eth0 (PRIMERO, gana)
├── 25-eth0-default.network  ← También coincide con eth0, pero IGNORADO
└── 90-catchall.network      ← Aplica a todo lo demás
```

Un patrón common es tener un archivo "catch-all" con prioridad baja:

```ini
# /etc/systemd/network/90-dhcp-all.network
# Fallback: cualquier interfaz ethernet usa DHCP
[Match]
Name=en*
Type=ether

[Network]
DHCP=yes
```

---

## 8. DHCP con systemd-networkd

### Configuración básica

```ini
# /etc/systemd/network/25-eth0.network
[Match]
Name=eth0

[Network]
DHCP=yes           # IPv4 + IPv6
# DHCP=ipv4        # Solo IPv4
# DHCP=ipv6        # Solo IPv6
# DHCP=no          # Sin DHCP (default)
```

### Opciones de [DHCPv4]

```ini
[DHCPv4]
# Qué usar del servidor DHCP
UseDNS=yes                    # Usar DNS del servidor (default: yes)
UseRoutes=yes                 # Usar rutas del servidor (default: yes)
UseHostname=no                # Aceptar hostname del servidor (default: yes)
UseNTP=yes                    # Usar NTP del servidor (default: yes)
UseDomains=yes                # Usar dominios de búsqueda
# UseDomains=route            # Usar como routing domains (para resolved)

# Qué enviar al servidor DHCP
SendHostname=yes              # Enviar nuestro hostname (default: yes)
Hostname=mi-servidor          # Hostname específico a enviar
ClientIdentifier=mac          # Identificador: mac o duid

# Lease
MaxLeaseTimeSec=86400         # No aceptar leases mayores a 24h

# Rutas
RouteMetric=100               # Metric para rutas aprendidas por DHCP
UseGateway=yes                # Usar gateway del servidor (default: yes)

# Comportamiento ante fallo
SendRelease=yes               # Enviar RELEASE al parar (default: yes)

# Solicitar opciones específicas (por número)
RequestOptions=1 3 6 12 15 26 28 42 119

# Opciones de reintento
MaxAttempts=infinity           # Reintentar indefinidamente (default)
```

### DHCP con DNS estático adicional

```ini
# /etc/systemd/network/25-eth0.network
[Match]
Name=eth0

[Network]
DHCP=yes
DNS=127.0.0.1                 # DNS local (cache, Pi-hole, etc.)
# Se combina con los DNS del DHCP

[DHCPv4]
UseDNS=yes                    # También usar DNS del DHCP
```

### DHCP sin ruta por defecto

Útil para interfaces secundarias donde no quieres que el tráfico de Internet salga por ahí:

```ini
# /etc/systemd/network/30-eth1.network
[Match]
Name=eth1

[Network]
DHCP=yes

[DHCPv4]
UseGateway=no                 # No instalar ruta por defecto
UseRoutes=yes                 # Pero sí las rutas de subred
```

---

## 9. IP estática con systemd-networkd

### Configuración básica

```ini
# /etc/systemd/network/25-eth0.network
[Match]
Name=eth0

[Network]
Address=192.168.1.50/24
Gateway=192.168.1.1
DNS=192.168.1.1
DNS=8.8.8.8
Domains=empresa.local
```

### Múltiples direcciones

```ini
[Network]
# Método 1: en la sección [Network]
Address=192.168.1.50/24
Address=10.0.0.1/24

# Método 2: secciones [Address] separadas (más control)
```

```ini
[Address]
Address=192.168.1.50/24
Label=eth0:main

[Address]
Address=10.0.0.1/24
Label=eth0:secondary
Peer=10.0.0.2/24              # Para enlaces punto a punto
```

### IPv4 + IPv6 estática

```ini
[Match]
Name=eth0

[Network]
Address=192.168.1.50/24
Address=2001:db8::50/64
Gateway=192.168.1.1
Gateway=2001:db8::1
DNS=192.168.1.1
DNS=2001:4860:4860::8888
```

### Solo link-local (sin IP global)

```ini
[Match]
Name=eth0

[Network]
LinkLocalAddressing=yes        # Solo IPv4 link-local (169.254.x.x)
# LinkLocalAddressing=ipv6     # Solo IPv6 link-local (fe80::)
# LinkLocalAddressing=no       # Sin link-local
LLDP=yes                       # Pero escuchar LLDP
```

---

## 10. Rutas, DNS y dominios

### Rutas estáticas

```ini
# Ruta simple
[Route]
Destination=10.0.0.0/8
Gateway=192.168.1.254

# Múltiples rutas: una sección [Route] por cada una
[Route]
Destination=10.0.0.0/8
Gateway=192.168.1.254
Metric=100

[Route]
Destination=172.16.0.0/12
Gateway=192.168.1.253
Metric=200

# Ruta por defecto explícita (equivale a Gateway= en [Network])
[Route]
Destination=0.0.0.0/0
Gateway=192.168.1.1
Metric=100

# Blackhole
[Route]
Destination=10.99.0.0/16
Type=blackhole

# Unreachable
[Route]
Destination=10.98.0.0/16
Type=unreachable

# Ruta con IP origen específica
[Route]
Destination=10.0.0.0/8
Gateway=192.168.1.254
PreferredSource=192.168.1.50
```

### DNS

```ini
[Network]
# DNS directo (siempre se usa, independiente de DHCP)
DNS=1.1.1.1
DNS=8.8.8.8

# Con systemd-resolved: DNS over TLS
DNS=1.1.1.1#cloudflare-dns.com
DNS=8.8.8.8#dns.google

# Dominios de búsqueda
Domains=empresa.local dev.internal

# Routing domains (con ~ prefix, para resolved)
Domains=~empresa.local
# "Las consultas para *.empresa.local van por los DNS de esta interfaz"

# Catch-all routing domain
Domains=~.
# "Todas las consultas DNS van por los DNS de esta interfaz"
```

### Diferencia entre search domains y routing domains

```
Search domain: "empresa.local"
  → Al buscar "servidor", resolved intenta "servidor.empresa.local"
  → Completa nombres cortos

Routing domain: "~empresa.local"  (con ~)
  → Las consultas para *.empresa.local se envían por los DNS
    configurados en ESTA interfaz
  → No completa nombres cortos
  → Útil para split DNS (VPN)

Catch-all: "~."
  → TODO el tráfico DNS va por los DNS de esta interfaz
  → Prioridad sobre otras interfaces
```

```
Ejemplo: split DNS con VPN

eth0 (Internet):
  DNS=8.8.8.8
  Domains=~.                    # Default para todo

tun0 (VPN corporativa):
  DNS=10.0.0.53
  Domains=~corp.empresa.com     # Solo *.corp.empresa.com va por aquí

Resultado:
  google.com      → 8.8.8.8 (eth0, catch-all)
  intranet.corp.empresa.com → 10.0.0.53 (tun0, routing domain)
```

---

## 11. networkctl — Control y diagnóstico

`networkctl` es la herramienta de línea de comandos para systemd-networkd (equivalente a `nmcli` para NetworkManager, pero más simple).

### Estado general

```bash
networkctl status
```

```
●        State: routable
  Online state: online
       Address: 192.168.1.100 on eth0
                fe80::a8bb:ccff:fe11:2233 on eth0
       Gateway: 192.168.1.1 on eth0
           DNS: 192.168.1.1
                8.8.8.8
Search Domains: home.local
```

### Estado de una interfaz

```bash
networkctl status eth0
```

```
● 2: eth0
             Link File: /usr/lib/systemd/network/99-default.link
          Network File: /etc/systemd/network/25-eth0.network
                  Type: ether
                 State: routable (configured)
          Online state: online
    Connected to: MySwitch on port 5 (via LLDP)
                Driver: e1000
            HW Address: aa:bb:cc:11:22:33
                   MTU: 1500
                 QDisc: fq_codel
  IPv6 Address Generation Mode: eui64
          Queue Length (Tx/Rx): 1/1
              Auto negotiation: yes
                         Speed: 1Gbps
                        Duplex: full
                          Port: tp
                       Address: 192.168.1.100 (DHCP4 via 192.168.1.1)
                                fe80::a8bb:ccff:fe11:2233
                       Gateway: 192.168.1.1
                           DNS: 192.168.1.1
                                8.8.8.8
                Search Domains: home.local
         Activation Policy: up
```

### Listar interfaces

```bash
networkctl list
```

```
IDX LINK    TYPE     OPERATIONAL SETUP
  1 lo      loopback carrier     unmanaged
  2 eth0    ether    routable    configured
  3 eth1    ether    off         unmanaged
  4 br0     bridge   routable    configured
```

**Estados OPERATIONAL**:

| Estado | Significado |
|--------|-------------|
| `off` | Interfaz desactivada |
| `no-carrier` | Sin cable/señal |
| `dormant` | Esperando evento externo |
| `carrier` | Cable conectado pero sin IP (o solo L2) |
| `degraded` | Tiene IP pero no ruta por defecto |
| `routable` | Totalmente funcional, con ruta a Internet |

**Estados SETUP**:

| Estado | Significado |
|--------|-------------|
| `unmanaged` | No gestionada por systemd-networkd |
| `configuring` | Aplicando configuración (DHCP en curso) |
| `configured` | Configuración aplicada correctamente |
| `failed` | Error al aplicar configuración |
| `linger` | La interfaz desapareció pero la config persiste |

### Acciones

```bash
# Renovar lease DHCP
sudo networkctl renew eth0

# Reconfigurar interfaz (relee archivos .network)
sudo networkctl reconfigure eth0

# Recargar todos los archivos de configuración
sudo networkctl reload

# Activar/desactivar interfaz
sudo networkctl up eth0
sudo networkctl down eth0

# Forzar reconfiguración de todo
sudo networkctl reconfigure --all
```

> **Orden importante**: `reload` solo relee los archivos. `reconfigure` aplica la nueva configuración a una interfaz. Típicamente necesitas ambos si cambiaste un archivo.

### LLDP (Link Layer Discovery Protocol)

```bash
# Ver vecinos LLDP (switches conectados)
networkctl lldp
```

```
LINK  CHASSIS ID        SYSTEM NAME  PORT ID    PORT DESCRIPTION  CAPS
eth0  00:11:22:33:44:55 MySwitch     Gi0/5      GigabitEthernet5  bridge,router
```

Esto es muy útil en data centers para identificar en qué puerto de qué switch está conectado cada servidor.

---

## 12. Integración con systemd-resolved

systemd-networkd y systemd-resolved están diseñados para trabajar juntos. La comunicación es automática vía D-Bus:

```
systemd-networkd                    systemd-resolved
       │                                   │
       │ Obtiene DNS vía DHCP              │
       │ o configuración estática          │
       │                                   │
       │──── D-Bus ───────────────────────►│
       │     "eth0: DNS=1.1.1.1,          │
       │      Domains=~empresa.local"      │
       │                                   │
       │                            ┌──────┴──────┐
       │                            │  Resuelve   │
       │                            │  consultas  │
       │                            │  DNS con    │
       │                            │  split DNS  │
       │                            │  por interfaz│
       │                            └─────────────┘
```

```bash
# Verificar que resolved recibe los DNS de networkd
resolvectl status eth0
```

```
Link 2 (eth0)
    Current Scopes: DNS LLMNR/IPv4
  Current DNS Server: 192.168.1.1
         DNS Servers: 192.168.1.1 8.8.8.8
          DNS Domain: home.local
```

### Configurar DNS over TLS con ambos

```ini
# /etc/systemd/network/25-eth0.network
[Match]
Name=eth0

[Network]
DHCP=yes
DNS=1.1.1.1#cloudflare-dns.com     # IP#hostname para DoT
DNS=9.9.9.9#dns.quad9.net

[DHCPv4]
UseDNS=no                           # No usar DNS del DHCP
```

```ini
# /etc/systemd/resolved.conf.d/dot.conf
[Resolve]
DNSOverTLS=yes                       # Forzar DNS over TLS
```

---

## 13. Configuraciones avanzadas

### Esperar a que la red esté lista (systemd-networkd-wait-online)

```bash
# Este servicio bloquea el boot hasta que la red esté configurada
systemctl status systemd-networkd-wait-online.service

# Configurar qué significa "online"
# /etc/systemd/system/systemd-networkd-wait-online.service.d/override.conf
[Service]
ExecStart=
ExecStart=/usr/lib/systemd/systemd-networkd-wait-online --any
# --any: basta con que UNA interfaz esté online
# Sin --any: TODAS las interfaces deben estar online (puede causar boot lento)
```

### Hotplug — interfaces que aparecen y desaparecen

```ini
# /etc/systemd/network/80-usb-ethernet.network
[Match]
Name=enx*            # Adaptadores USB ethernet (nombre basado en MAC)
Type=ether

[Network]
DHCP=yes

[DHCPv4]
RouteMetric=200       # Menor prioridad que la interfaz principal
```

### Configuración condicional por entorno

```ini
# /etc/systemd/network/25-vm-only.network
[Match]
Name=eth0
Virtualization=vm          # Solo aplica dentro de una VM

[Network]
DHCP=yes

[DHCPv4]
SendHostname=no            # No revelar hostname en entornos cloud
```

```ini
# /etc/systemd/network/25-container.network
[Match]
Name=host0
Virtualization=container   # Solo en contenedores systemd-nspawn

[Network]
DHCP=yes
```

### Política de activación

```ini
[Link]
ActivationPolicy=up              # Siempre UP (default)
# ActivationPolicy=always-up     # UP incluso si se intenta bajar
# ActivationPolicy=manual        # Solo UP si se activa explícitamente
# ActivationPolicy=always-down   # Siempre DOWN
# ActivationPolicy=down          # DOWN inicialmente, puede subirse
# ActivationPolicy=bound         # UP solo si la interfaz master está UP
```

### RequiredForOnline

```ini
# /etc/systemd/network/25-eth0.network
[Match]
Name=eth0

[Link]
RequiredForOnline=yes              # Esta interfaz debe estar online para
                                   # que systemd-networkd-wait-online termine
# RequiredForOnline=no             # Opcional, no bloquea el boot
# RequiredForOnline=routable       # Debe llegar a estado "routable"
```

---

## 14. Errores comunes

### Error 1: Crear archivos .network pero no activar systemd-networkd

```
INCORRECTO:
# Crear el archivo de configuración
sudo vim /etc/systemd/network/25-eth0.network
# "¿Por qué no funciona?"

# Verificar:
systemctl is-active systemd-networkd
# → inactive

CORRECTO:
# Activar y arrancar systemd-networkd
sudo systemctl enable --now systemd-networkd

# Y si quieres DNS:
sudo systemctl enable --now systemd-resolved
```

### Error 2: Olvidar reload + reconfigure después de editar

```
INCORRECTO:
sudo vim /etc/systemd/network/25-eth0.network
# Cambié la IP
# "¿Por qué sigue con la IP anterior?"

CORRECTO:
sudo vim /etc/systemd/network/25-eth0.network
sudo networkctl reload                  # Relee los archivos
sudo networkctl reconfigure eth0        # Aplica cambios a la interfaz

# O si prefieres reiniciar todo:
sudo systemctl restart systemd-networkd
# (breve interrupción de red)
```

### Error 3: Múltiples archivos .network coinciden con la misma interfaz

```
INCORRECTO:
# /etc/systemd/network/25-eth0.network
[Match]
Name=eth0
[Network]
Address=192.168.1.50/24

# /etc/systemd/network/90-all.network
[Match]
Name=en*
[Network]
DHCP=yes

# "eth0 tiene IP estática Y DHCP" → NO, solo 25-eth0 aplica (el primero)
# El 90-all.network se ignora para eth0

CORRECTO:
# Solo se aplica el PRIMER archivo que coincide (orden alfanumérico)
# 25 < 90, así que 25-eth0.network gana para eth0
# 90-all.network aplica a otras interfaces en* que no tienen
# un archivo más específico

# Si quieres DHCP además de la IP estática en eth0,
# ponlo todo en el mismo archivo:
[Network]
DHCP=yes
Address=192.168.1.50/24    # IP extra además de la del DHCP
```

### Error 4: Boot lento por systemd-networkd-wait-online

```
PROBLEMA:
# El sistema tarda 2 minutos en arrancar
# systemd-analyze blame muestra:
# 2min 0.001s systemd-networkd-wait-online.service

CAUSA:
# wait-online espera a que TODAS las interfaces estén online
# Una interfaz sin cable (eth1) nunca llega a "online"

CORRECTO:
# Opción 1: solo esperar a que UNA interfaz esté online
sudo mkdir -p /etc/systemd/system/systemd-networkd-wait-online.service.d/
sudo tee /etc/systemd/system/systemd-networkd-wait-online.service.d/override.conf <<'EOF'
[Service]
ExecStart=
ExecStart=/usr/lib/systemd/systemd-networkd-wait-online --any
EOF

# Opción 2: marcar interfaces opcionales
# En el .network de eth1:
[Link]
RequiredForOnline=no

# Opción 3: especificar qué interfaz esperar
# ExecStart=/usr/lib/systemd/systemd-networkd-wait-online -i eth0
```

### Error 5: Confundir Gateway= en [Network] con Gateway= en [Route]

```
INCORRECTO:
[Network]
Gateway=192.168.1.1       # Ruta por defecto, metric=1024 (default alto)

[Route]
Gateway=192.168.1.1       # ¿Ruta a dónde? Sin Destination= es otra default route
# Dos rutas por defecto → conflicto potencial

CORRECTO:
# Usar UNO u otro, no ambos:

# Opción 1: simple (Gateway en [Network])
[Network]
Address=192.168.1.50/24
Gateway=192.168.1.1         # Ruta por defecto, simple

# Opción 2: con control (sección [Route])
[Network]
Address=192.168.1.50/24

[Route]
Destination=0.0.0.0/0       # Explícito: ruta por defecto
Gateway=192.168.1.1
Metric=100                   # Control de prioridad
```

---

## 15. Cheatsheet

```
ARCHIVOS DE CONFIGURACIÓN
──────────────────────────────────────────────
/etc/systemd/network/*.network     Config de IP/DHCP/DNS
/etc/systemd/network/*.netdev      Crear interfaces virtuales
/etc/systemd/network/*.link        Propiedades de hardware (udev)

Prioridad: /etc/ > /run/ > /usr/lib/
Orden: alfanumérico (10- < 20- < 90-)
Solo aplica el PRIMER .network que coincida

ESTRUCTURA MÍNIMA
──────────────────────────────────────────────
DHCP:                    Estática:
[Match]                  [Match]
Name=eth0                Name=eth0
[Network]                [Network]
DHCP=yes                 Address=IP/M
                         Gateway=GW
                         DNS=DNS

SECCIONES CLAVE
──────────────────────────────────────────────
[Match]      Name=, Type=, MACAddress=, Driver=
[Network]    DHCP=, Address=, Gateway=, DNS=, Domains=
[DHCPv4]     UseDNS=, UseRoutes=, SendHostname=, RouteMetric=
[Address]    Address=, Label=
[Route]      Destination=, Gateway=, Metric=, Type=
[Link]       MTUBytes=, RequiredForOnline=

NETWORKCTL
──────────────────────────────────────────────
networkctl list              Listar interfaces
networkctl status            Estado general
networkctl status eth0       Estado de interfaz
networkctl lldp              Vecinos LLDP
networkctl renew eth0        Renovar DHCP
networkctl reload            Recargar archivos
networkctl reconfigure eth0  Aplicar cambios
networkctl up/down eth0      Activar/desactivar

INTERFAZ VIRTUAL (.netdev)
──────────────────────────────────────────────
Kind=bridge     Bridge (br0)
Kind=vlan       VLAN 802.1Q
Kind=bond       Bonding/LACP
Kind=veth       Par virtual ethernet
Kind=dummy      Interfaz dummy (testing)
Kind=wireguard  Túnel WireGuard

LOGS
──────────────────────────────────────────────
journalctl -u systemd-networkd      Logs del daemon
journalctl -u systemd-networkd -f   Seguir en tiempo real
networkctl status eth0               Estado con detalles
```

---

## 16. Ejercicios

### Ejercicio 1: Configurar una interfaz con systemd-networkd

En una VM o contenedor donde systemd-networkd sea el gestor activo:

1. Verifica que systemd-networkd está activo (`systemctl is-active systemd-networkd`).
2. Lista las interfaces con `networkctl list`. ¿Cuáles están "configured" y cuáles "unmanaged"?
3. Busca el archivo `.network` que configura tu interfaz principal (`networkctl status eth0` muestra "Network File:").
4. Muestra el estado completo de la interfaz con `networkctl status eth0`. Identifica: IP, gateway, DNS, si es DHCP o estática, y qué archivo `.network` se aplica.
5. Si tienes DHCP, renueva el lease con `networkctl renew eth0`. Verifica en `journalctl -u systemd-networkd --since "1 min ago"` que la renovación ocurrió.

### Ejercicio 2: Diseñar configuración para un servidor multi-interfaz

Un servidor tiene tres interfaces: `eth0` (Internet), `eth1` (red interna), `eth2` (red de gestión). Diseña los archivos `.network` para:

- **eth0**: DHCP, DNS del DHCP, ruta por defecto, metric 100
- **eth1**: IP estática 10.0.0.1/24, sin gateway (red local), DNS propio 10.0.0.53, dominio "internal.corp"
- **eth2**: IP estática 172.16.0.10/24, sin gateway, no requerida para boot online

Escribe los tres archivos `.network` con nombres y prefijos apropiados. Luego responde:
1. ¿Por qué eth0 tiene el prefijo más bajo?
2. Si agregas un archivo `90-catchall.network` con `[Match] Name=en*` y `DHCP=yes`, ¿afecta a eth0, eth1 o eth2?
3. ¿Cómo configurarías eth1 para que las consultas DNS para `*.internal.corp` vayan por 10.0.0.53 pero todo lo demás use el DNS de eth0?

### Ejercicio 3: Migrar de NetworkManager a systemd-networkd

Tienes un servidor con NetworkManager que tiene esta configuración (obtenida con `nmcli connection show "Server"`):

```
ipv4.method:                    manual
ipv4.addresses:                 192.168.1.50/24
ipv4.gateway:                   192.168.1.1
ipv4.dns:                       192.168.1.1,8.8.8.8
ipv4.dns-search:                lab.local
ipv4.routes:                    10.0.0.0/8 192.168.1.254
802-3-ethernet.mtu:             9000
```

1. Escribe el archivo `.network` equivalente para systemd-networkd.
2. Describe los pasos exactos para migrar sin perder conectividad (considerando que estás conectado por SSH).
3. ¿Qué verificarías después de la migración para confirmar que todo funciona?
4. ¿Cómo revertirías si algo sale mal?

**Pregunta de reflexión**: Un equipo DevOps gestiona 300 servidores con Ansible. Actualmente usan NetworkManager. Un ingeniero propone migrar a systemd-networkd porque "los archivos `.network` son más fáciles de gestionar con Ansible que `nmcli`". ¿Estás de acuerdo? ¿Qué factores considerarías para tomar esta decisión? ¿Existe algún riesgo al migrar servidores en producción?

---

> **Siguiente tema**: T04 — /etc/network/interfaces (configuración clásica Debian)
