# T03 — Configuración de Cliente DHCP: dhclient, NetworkManager y systemd-networkd

## Índice

1. [Panorama de clientes DHCP en Linux](#1-panorama-de-clientes-dhcp-en-linux)
2. [dhclient (ISC DHCP client)](#2-dhclient-isc-dhcp-client)
3. [NetworkManager como cliente DHCP](#3-networkmanager-como-cliente-dhcp)
4. [systemd-networkd como cliente DHCP](#4-systemd-networkd-como-cliente-dhcp)
5. [Comparativa de los tres clientes](#5-comparativa-de-los-tres-clientes)
6. [Configuración de hostname vía DHCP](#6-configuración-de-hostname-vía-dhcp)
7. [Hooks y scripts post-DHCP](#7-hooks-y-scripts-post-dhcp)
8. [IP estática vs DHCP: cuándo elegir cada uno](#8-ip-estática-vs-dhcp-cuándo-elegir-cada-uno)
9. [Diagnóstico del cliente DHCP](#9-diagnóstico-del-cliente-dhcp)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Panorama de clientes DHCP en Linux

En Linux no existe un único cliente DHCP. A lo largo de los años han coexistido varias implementaciones, y la que tu sistema usa depende de la distribución y del gestor de red activo:

```
Evolución de clientes DHCP en Linux:

pump / dhcpcd (legacy)
  │
  ├── dhclient (ISC)          ← Clásico, aún presente
  │     Usado por: ifupdown, NetworkManager (antes)
  │
  ├── NetworkManager           ← Desktop, cliente interno
  │     Tiene su propio cliente DHCP integrado
  │
  └── systemd-networkd         ← Servidores, contenedores
        Tiene su propio cliente DHCP integrado
```

```
¿Quién gestiona DHCP en tu sistema?

┌──────────────────────────────────────────────────┐
│              Aplicación (Firefox, curl)           │
├──────────────────────────────────────────────────┤
│              Kernel (interfaz de red)             │
├──────────────┬──────────────┬────────────────────┤
│  ifupdown +  │  Network-    │  systemd-          │
│  dhclient    │  Manager     │  networkd          │
│              │  (interno)   │  (interno)         │
│  Debian      │  Fedora,     │  Servidores,       │
│  clásico     │  Ubuntu      │  contenedores,     │
│              │  desktop     │  Ubuntu server     │
└──────────────┴──────────────┴────────────────────┘
```

**Regla fundamental**: solo un gestor de red debe controlar cada interfaz. Si NetworkManager y systemd-networkd intentan gestionar la misma interfaz, habrá conflictos (IPs duplicadas, rutas que desaparecen, logs confusos).

```bash
# ¿Qué gestor de red está activo en mi sistema?

# NetworkManager
systemctl is-active NetworkManager

# systemd-networkd
systemctl is-active systemd-networkd

# Verificar que no estén ambos activos para las mismas interfaces
```

---

## 2. dhclient (ISC DHCP client)

`dhclient` es el cliente DHCP clásico del proyecto ISC (Internet Systems Consortium). Aunque ISC anunció el fin de vida de su suite DHCP en favor de Kea, dhclient sigue siendo muy usado en sistemas Debian y como backend de NetworkManager en algunas configuraciones.

### 2.1 Uso básico

```bash
# Obtener IP por DHCP en una interfaz
sudo dhclient eth0

# Liberar el lease actual
sudo dhclient -r eth0

# Liberar y obtener nuevo lease
sudo dhclient -r eth0 && sudo dhclient eth0

# Obtener lease mostrando todo el proceso
sudo dhclient -v eth0

# Modo debug (máximo detalle)
sudo dhclient -d eth0
# -d también mantiene dhclient en foreground (no se demoniza)
```

Salida típica con `-v`:

```
Internet Systems Consortium DHCP Client 4.4.3
Listening on LPF/eth0/aa:bb:cc:11:22:33
Sending on   LPF/eth0/aa:bb:cc:11:22:33
DHCPDISCOVER on eth0 to 255.255.255.255 port 67 interval 3
DHCPOFFER of 192.168.1.100 from 192.168.1.1
DHCPREQUEST for 192.168.1.100 on eth0 to 255.255.255.255 port 67
DHCPACK of 192.168.1.100 from 192.168.1.1
bound to 192.168.1.100 -- renewal in 14400 seconds.
```

### 2.2 Archivo de configuración: dhclient.conf

```bash
# Ubicación principal
/etc/dhcp/dhclient.conf

# O por interfaz
/etc/dhcp/dhclient-eth0.conf
```

```
# /etc/dhcp/dhclient.conf

# Tiempo que espera por una oferta antes de rendirse
timeout 60;

# Tiempo de reintento si falla
retry 300;

# Opciones que el cliente solicita al servidor
request subnet-mask, broadcast-address, time-offset, routers,
        domain-name, domain-name-servers, domain-search,
        host-name, ntp-servers;

# Opciones que el cliente envía
send host-name = gethostname();
send dhcp-client-identifier 01:aa:bb:cc:11:22:33;

# Forzar un lease time específico (el servidor puede ignorarlo)
send dhcp-lease-time 3600;

# Anteponer un servidor DNS propio a los que envía el servidor
prepend domain-name-servers 127.0.0.1;

# Sobrescribir el dominio de búsqueda (ignorar lo que envíe el servidor)
supersede domain-search "miempresa.local";

# No aceptar ofertas de un servidor específico (rogue DHCP)
reject 192.168.1.254;
```

Las directivas `prepend`, `append`, `supersede` y `default` controlan cómo se combinan las opciones del servidor con las preferencias del cliente:

| Directiva | Efecto |
|-----------|--------|
| `supersede` | Ignora completamente lo que envíe el servidor |
| `prepend` | Añade valores ANTES de los del servidor |
| `append` | Añade valores DESPUÉS de los del servidor |
| `default` | Usa el valor solo si el servidor no envía esa opción |

```
Ejemplo práctico — DNS con prepend:

Servidor DHCP envía: DNS = 192.168.1.1, 8.8.8.8
dhclient.conf tiene: prepend domain-name-servers 127.0.0.1;

Resultado en /etc/resolv.conf:
  nameserver 127.0.0.1    ← prepend (local DNS cache)
  nameserver 192.168.1.1  ← del servidor
  nameserver 8.8.8.8      ← del servidor
```

### 2.3 Archivos de lease del cliente

```bash
# Lease obtenido (lo que el servidor otorgó)
/var/lib/dhcp/dhclient.leases
/var/lib/dhcp/dhclient.eth0.leases

# Ver el lease actual
cat /var/lib/dhcp/dhclient.eth0.leases
```

```
lease {
  interface "eth0";
  fixed-address 192.168.1.100;
  option subnet-mask 255.255.255.0;
  option routers 192.168.1.1;
  option domain-name-servers 192.168.1.1, 8.8.8.8;
  option domain-name "home.local";
  option dhcp-lease-time 28800;
  option dhcp-message-type 5;
  option dhcp-server-identifier 192.168.1.1;
  renew 6 2026/03/21 12:00:00;
  rebind 6 2026/03/21 15:00:00;
  expire 6 2026/03/21 16:00:00;
}
```

### 2.4 Scripts de hook (dhclient-script)

Cuando dhclient obtiene o pierde un lease, ejecuta un script que aplica la configuración al sistema:

```bash
# Script principal
/sbin/dhclient-script

# Hooks adicionales (se ejecutan en orden alfabético)
/etc/dhcp/dhclient-enter-hooks.d/   # Antes de aplicar config
/etc/dhcp/dhclient-exit-hooks.d/    # Después de aplicar config
```

El script recibe información a través de variables de entorno:

```bash
# Variables disponibles en los hooks:
# $reason     - BOUND, RENEW, REBIND, REBOOT, EXPIRE, RELEASE, etc.
# $interface  - Nombre de la interfaz (eth0)
# $new_ip_address       - IP obtenida
# $new_subnet_mask      - Máscara
# $new_routers          - Gateway(s)
# $new_domain_name_servers - DNS
# $old_ip_address       - IP anterior (en renovación)
```

```bash
# Ejemplo: hook que registra cambios de IP
# /etc/dhcp/dhclient-exit-hooks.d/log-changes

case "$reason" in
    BOUND|RENEW|REBIND|REBOOT)
        logger -t dhclient "Interface $interface got IP $new_ip_address"
        ;;
    EXPIRE|RELEASE)
        logger -t dhclient "Interface $interface lost IP $old_ip_address"
        ;;
esac
```

---

## 3. NetworkManager como cliente DHCP

NetworkManager es el gestor de red estándar en la mayoría de distribuciones desktop (Fedora, Ubuntu Desktop, RHEL workstation). Gestiona DHCP internamente sin necesidad de dhclient externo.

### 3.1 Cliente DHCP interno vs externo

NetworkManager puede usar diferentes backends DHCP:

```
NetworkManager DHCP backends:

┌─────────────────────┐
│   NetworkManager    │
│                     │
│  ┌───────────────┐  │
│  │   internal    │  │  ← Por defecto desde ~2018
│  │  (integrado)  │  │     No necesita dhclient
│  └───────────────┘  │
│                     │
│  ┌───────────────┐  │
│  │   dhclient    │  │  ← Legacy, aún soportado
│  │  (externo)    │  │     Requiere dhclient instalado
│  └───────────────┘  │
└─────────────────────┘
```

```ini
# /etc/NetworkManager/NetworkManager.conf
[main]
# Backend DHCP: internal (default) o dhclient
dhcp=internal
```

```bash
# Verificar qué backend usa
grep -i dhcp /etc/NetworkManager/NetworkManager.conf

# Si no aparece nada, usa "internal" (el default)
```

### 3.2 Configurar DHCP con nmcli

```bash
# Ver conexiones existentes
nmcli connection show

# Ver configuración de una conexión
nmcli connection show "Wired connection 1"

# Configurar interfaz para DHCP (método automático)
nmcli connection modify "Wired connection 1" ipv4.method auto

# Aplicar cambios sin desconectar
nmcli connection up "Wired connection 1"

# O recargar la interfaz
nmcli device reapply eth0
```

### 3.3 Opciones DHCP avanzadas con nmcli

```bash
# Enviar hostname al servidor DHCP
nmcli connection modify "Wired connection 1" \
    ipv4.dhcp-hostname "mi-laptop"

# Enviar client-id personalizado
nmcli connection modify "Wired connection 1" \
    ipv4.dhcp-client-id "mi-identificador"

# Solicitar un lease time específico (no todos los servidores lo respetan)
nmcli connection modify "Wired connection 1" \
    ipv4.dhcp-timeout 45

# NO usar la ruta por defecto que envía el servidor
# (útil para VPNs o configuraciones multi-gateway)
nmcli connection modify "Wired connection 1" \
    ipv4.never-default yes

# Agregar DNS adicionales además de los del DHCP
nmcli connection modify "Wired connection 1" \
    ipv4.dns "127.0.0.1" \
    ipv4.dns-priority -1

# Ignorar el DNS que envía el DHCP (usar solo los configurados manualmente)
nmcli connection modify "Wired connection 1" \
    ipv4.ignore-auto-dns yes
```

### 3.4 Ver información de lease

```bash
# Información DHCP de una interfaz
nmcli -f DHCP4 device show eth0
```

```
DHCP4.OPTION[1]:  dhcp-lease-time = 28800
DHCP4.OPTION[2]:  dhcp-server-identifier = 192.168.1.1
DHCP4.OPTION[3]:  domain_name = home.local
DHCP4.OPTION[4]:  domain_name_servers = 192.168.1.1 8.8.8.8
DHCP4.OPTION[5]:  expiry = 1742637600
DHCP4.OPTION[6]:  ip_address = 192.168.1.100
DHCP4.OPTION[7]:  next_server = 192.168.1.1
DHCP4.OPTION[8]:  requested_broadcast_address = 1
DHCP4.OPTION[9]:  routers = 192.168.1.1
DHCP4.OPTION[10]: subnet_mask = 255.255.255.0
```

```bash
# Archivo de lease interno
ls /var/lib/NetworkManager/internal-*.lease

# También disponible vía D-Bus (para herramientas de escritorio)
busctl get-property org.freedesktop.NetworkManager \
    /org/freedesktop/NetworkManager/DHCP4Config/1 \
    org.freedesktop.NetworkManager.DHCP4Config Options
```

### 3.5 Forzar renovación

```bash
# Método 1: reapply (no interrumpe la conexión)
nmcli device reapply eth0

# Método 2: desconectar y reconectar
nmcli device disconnect eth0
nmcli device connect eth0

# Método 3: reactivar la conexión
nmcli connection down "Wired connection 1"
nmcli connection up "Wired connection 1"
```

### 3.6 Dispatcher scripts

NetworkManager tiene su propio sistema de hooks llamado **dispatcher**:

```bash
# Directorio de scripts del dispatcher
/etc/NetworkManager/dispatcher.d/

# Scripts que se ejecutan antes de la acción
/etc/NetworkManager/dispatcher.d/pre-up.d/
/etc/NetworkManager/dispatcher.d/pre-down.d/
```

```bash
#!/bin/bash
# /etc/NetworkManager/dispatcher.d/99-dhcp-log
# Debe tener permisos 755 y ser propiedad de root

INTERFACE=$1
ACTION=$2

case "$ACTION" in
    dhcp4-change)
        logger -t nm-dispatcher "DHCP4 change on $INTERFACE: IP=$DHCP4_IP_ADDRESS"
        ;;
    up)
        logger -t nm-dispatcher "$INTERFACE is up"
        ;;
    down)
        logger -t nm-dispatcher "$INTERFACE is down"
        ;;
esac
```

Acciones del dispatcher relacionadas con DHCP:

| Acción | Cuándo se ejecuta |
|--------|-------------------|
| `dhcp4-change` | Cuando se obtiene o renueva un lease DHCPv4 |
| `dhcp6-change` | Cuando se obtiene o renueva un lease DHCPv6 |
| `up` | Cuando la conexión se activa completamente |
| `down` | Cuando la conexión se desactiva |

---

## 4. systemd-networkd como cliente DHCP

systemd-networkd es el gestor de red integrado en systemd. Es ligero, sin dependencias gráficas, ideal para servidores y contenedores. Incluye su propio cliente DHCP.

### 4.1 Configuración básica

Los archivos de configuración van en `/etc/systemd/network/` con extensión `.network`:

```ini
# /etc/systemd/network/20-wired.network

[Match]
Name=eth0
# También soporta: Name=en*, MACAddress=aa:bb:cc:*, Type=ether

[Network]
DHCP=yes
# Opciones: yes (v4+v6), ipv4, ipv6, no

[DHCPv4]
# Opciones específicas del cliente DHCPv4
# (ver sección siguiente)
```

```bash
# Activar y arrancar systemd-networkd
sudo systemctl enable --now systemd-networkd

# Verificar estado
networkctl status
networkctl status eth0
```

Salida de `networkctl status eth0`:

```
● 2: eth0
             Link File: /usr/lib/systemd/network/99-default.link
          Network File: /etc/systemd/network/20-wired.network
                  Type: ether
                 State: routable (configured)
                Driver: virtio_net
            HW Address: aa:bb:cc:11:22:33
                   MTU: 1500
                 QDisc: fq_codel
  IPv6 Address Generation Mode: eui64
          Queue Length (Tx/Rx): 1/1
              Auto negotiation: no
                         Speed: n/a
                       Address: 192.168.1.100 (DHCP4 via 192.168.1.1)
                       Gateway: 192.168.1.1
                           DNS: 192.168.1.1
                                8.8.8.8
                Search Domains: home.local
```

### 4.2 Opciones de la sección [DHCPv4]

```ini
# /etc/systemd/network/20-wired.network

[Match]
Name=eth0

[Network]
DHCP=yes

[DHCPv4]
# Enviar hostname al servidor
SendHostname=yes
Hostname=mi-servidor

# Enviar Client Identifier
ClientIdentifier=mac
# Opciones: mac, duid, duid-only

# Solicitar opciones específicas
RequestOptions=1 3 6 12 15 26 28 42 119

# Usar el DNS que envía el servidor
UseDNS=yes

# Usar el gateway que envía el servidor
UseRoutes=yes

# Usar el NTP que envía el servidor
UseNTP=yes

# Usar el hostname que envía el servidor
UseHostname=yes

# Usar el dominio de búsqueda que envía el servidor
UseDomains=yes
# Opciones: yes, no, route (solo para routing domains en resolved)

# Duración máxima que aceptaremos para un lease
MaxLeaseTimeSec=86400

# Metric para las rutas instaladas por DHCP
RouteMetric=100

# Activar envío del Release al parar
SendRelease=yes

# Fallback a link-local si DHCP falla
FallbackLeaseLifetimeSec=forever
# Con esto, si DHCP falla, mantiene la última IP obtenida
```

### 4.3 Archivos de lease

```bash
# systemd-networkd almacena leases en:
/run/systemd/netif/leases/

# El nombre es el índice de la interfaz
# (ip link show te dice el índice: "2: eth0:")
cat /run/systemd/netif/leases/2
```

```ini
# Contenido típico:
[DHCPv4]
ADDRESS=192.168.1.100
NETMASK=255.255.255.0
ROUTER=192.168.1.1
DNS=192.168.1.1 8.8.8.8
DOMAINNAME=home.local
LIFETIME=28800
T1=14400
T2=25200
SERVER_ADDRESS=192.168.1.1
NEXT_SERVER=0.0.0.0
```

> **Nota**: estos archivos están en `/run/` (tmpfs), lo que significa que **se pierden al reiniciar**. Sin embargo, systemd-networkd guarda una copia persistente en `/var/lib/systemd/network/` para poder hacer INIT-REBOOT tras un reinicio.

### 4.4 Renovación y control

```bash
# Forzar renovación del lease
sudo networkctl renew eth0

# Reconfigurar la interfaz (relee los archivos .network)
sudo networkctl reconfigure eth0

# Recargar todos los archivos de configuración
sudo networkctl reload

# Ver logs del cliente DHCP
journalctl -u systemd-networkd -f | grep -i dhcp
```

### 4.5 Combinación con systemd-resolved

systemd-networkd se integra nativamente con systemd-resolved para DNS:

```
Flujo completo:

systemd-networkd                    systemd-resolved
    │                                      │
    │  Obtiene lease DHCP                  │
    │  (DNS=192.168.1.1, 8.8.8.8)         │
    │                                      │
    │──── Configura DNS por interfaz ─────►│
    │     vía D-Bus                        │
    │                                      │
    │                               ┌──────┴──────┐
    │                               │ 127.0.0.53  │
    │                               │ stub resolver│
    │                               └─────────────┘
```

```ini
# Para que systemd-networkd pase los DNS a resolved:
[Network]
DHCP=yes
DNS=127.0.0.1          # DNS adicional (local cache, por ejemplo)

[DHCPv4]
UseDNS=yes             # Pasar DNS del DHCP a resolved
UseDomains=route       # Usar dominios como routing domains
```

---

## 5. Comparativa de los tres clientes

```
┌────────────────────┬──────────────┬────────────────┬─────────────────┐
│ Característica     │  dhclient    │ NetworkManager │ systemd-networkd│
├────────────────────┼──────────────┼────────────────┼─────────────────┤
│ Tipo               │ Standalone   │ Integrado      │ Integrado       │
│ Config             │ dhclient.conf│ nmcli/GUI/conf │ .network files  │
│ Hooks              │ Shell scripts│ Dispatcher     │ Limitados       │
│ GUI                │ No           │ Sí (nm-applet) │ No              │
│ WiFi               │ No (solo IP) │ Sí (integrado) │ Necesita wpa_s. │
│ VPN                │ No           │ Sí (plugins)   │ No              │
│ Huella memoria     │ Baja         │ Media-alta     │ Muy baja        │
│ Lease file         │ /var/lib/dhcp│ /var/lib/NM/   │ /run/systemd/   │
│ DHCPv6             │ Sí (v6)      │ Sí             │ Sí              │
│ Mantenimiento      │ EOL (ISC)    │ Activo (GNOME) │ Activo (systemd)│
│ Distribuciones     │ Debian old   │ Fedora, Ubuntu │ Ubuntu Server,  │
│                    │              │ Desktop, RHEL  │ contenedores    │
└────────────────────┴──────────────┴────────────────┴─────────────────┘
```

Recomendaciones:

| Escenario | Cliente recomendado | Razón |
|-----------|---------------------|-------|
| Desktop con WiFi + VPN | NetworkManager | Gestión integrada de todo |
| Servidor headless | systemd-networkd | Ligero, declarativo, sin GUI |
| Contenedores | systemd-networkd o ninguno | El runtime del contenedor gestiona la red |
| Sistemas legacy | dhclient | Compatibilidad, scripts existentes |
| Red compleja (bonding, VLAN) | NetworkManager o systemd-networkd | Ambos soportan configuración avanzada |

---

## 6. Configuración de hostname vía DHCP

El servidor DHCP puede enviar un hostname al cliente (option 12) y el cliente puede enviar su hostname al servidor. Este intercambio tiene implicaciones importantes:

```
Flujo del hostname:

Cliente → Servidor (option 12 en DHCPREQUEST):
  "Mi nombre es laptop-usuario"
  → El servidor puede registrarlo en DNS dinámico (DDNS)
  → Aparece en el archivo de leases como client-hostname

Servidor → Cliente (option 12 en DHCPACK):
  "Tu nombre debería ser workstation-42"
  → El cliente puede aceptar o ignorar
```

```bash
# dhclient: enviar hostname
# En /etc/dhcp/dhclient.conf:
send host-name = gethostname();    # Envía el hostname actual del sistema
send host-name = "mi-nombre";     # Envía un nombre específico

# dhclient: aceptar o rechazar hostname del servidor
# supersede host-name "mi-nombre";  # Ignorar lo que envíe el servidor

# NetworkManager: enviar hostname
nmcli connection modify "conn" ipv4.dhcp-hostname "mi-laptop"

# NetworkManager: NO enviar hostname
nmcli connection modify "conn" ipv4.dhcp-send-hostname no

# systemd-networkd: enviar hostname
# En el archivo .network:
[DHCPv4]
SendHostname=yes
Hostname=mi-servidor

# systemd-networkd: aceptar hostname del servidor
[DHCPv4]
UseHostname=yes    # default: yes
```

> **Cuidado con la privacidad**: enviar el hostname por DHCP revela información del dispositivo en la red. En redes públicas (WiFi de cafetería), considera desactivar el envío de hostname.

---

## 7. Hooks y scripts post-DHCP

Los hooks permiten ejecutar acciones automáticas cuando el estado DHCP cambia. Cada cliente tiene su propio mecanismo:

### Comparativa de sistemas de hooks

```
dhclient hooks:
  /etc/dhcp/dhclient-enter-hooks.d/   ← Antes de aplicar
  /etc/dhcp/dhclient-exit-hooks.d/    ← Después de aplicar
  Variables: $reason, $new_ip_address, $interface, etc.

NetworkManager dispatcher:
  /etc/NetworkManager/dispatcher.d/   ← Scripts con $1=iface, $2=action
  Acciones DHCP: dhcp4-change, dhcp6-change
  Variables: $DHCP4_IP_ADDRESS, $DHCP4_ROUTERS, etc.

systemd-networkd:
  No tiene hooks propios
  Alternativa: systemd path unit que vigile /run/systemd/netif/leases/
```

### Caso práctico: actualizar /etc/hosts al recibir IP

```bash
#!/bin/bash
# /etc/NetworkManager/dispatcher.d/10-update-hosts
# Permisos: chmod 755, owner root:root

INTERFACE=$1
ACTION=$2

if [ "$ACTION" = "dhcp4-change" ] && [ "$INTERFACE" = "eth0" ]; then
    HOSTNAME=$(hostname)
    IP=$DHCP4_IP_ADDRESS

    # Eliminar entrada anterior para este hostname
    sed -i "/\s${HOSTNAME}$/d" /etc/hosts

    # Agregar nueva entrada
    echo "$IP $HOSTNAME" >> /etc/hosts

    logger -t nm-hosts "Updated /etc/hosts: $IP $HOSTNAME"
fi
```

### Caso práctico: notificar cambio de IP

```bash
#!/bin/bash
# /etc/dhcp/dhclient-exit-hooks.d/notify-ip-change

case "$reason" in
    BOUND|RENEW|REBIND)
        if [ "$old_ip_address" != "$new_ip_address" ]; then
            logger -t dhclient-hook \
                "IP changed on $interface: $old_ip_address -> $new_ip_address"
        fi
        ;;
esac
```

---

## 8. IP estática vs DHCP: cuándo elegir cada uno

```
                    DHCP                          IP Estática
                ┌───────────┐                 ┌───────────────┐
                │ Automático│                 │ Manual        │
                │ Centraliz.│                 │ Por máquina   │
                └─────┬─────┘                 └───────┬───────┘
                      │                               │
    Ideal para:       │                 Ideal para:   │
    ├── Workstations  │                 ├── Servidores│con IP conocida
    ├── Laptops       │                 ├── Routers  │
    ├── Teléfonos     │                 ├── Switches │gestionables
    ├── IoT           │                 └── Firewalls│
    └── VMs temporales│
                      │
    Término medio:    │
    DHCP con reserva ─┘
    (automático + IP predecible)
```

### Configurar IP estática con cada gestor

```bash
# --- NetworkManager ---
nmcli connection modify "Wired connection 1" \
    ipv4.method manual \
    ipv4.addresses "192.168.1.10/24" \
    ipv4.gateway "192.168.1.1" \
    ipv4.dns "192.168.1.1,8.8.8.8"
nmcli connection up "Wired connection 1"

# Volver a DHCP:
nmcli connection modify "Wired connection 1" ipv4.method auto
nmcli connection up "Wired connection 1"
```

```ini
# --- systemd-networkd ---
# /etc/systemd/network/20-wired.network

[Match]
Name=eth0

[Network]
Address=192.168.1.10/24
Gateway=192.168.1.1
DNS=192.168.1.1
DNS=8.8.8.8

# Volver a DHCP: cambiar a
# [Network]
# DHCP=yes
```

```
# --- ifupdown (Debian clásico) ---
# /etc/network/interfaces

# DHCP:
auto eth0
iface eth0 inet dhcp

# Estática:
auto eth0
iface eth0 inet static
    address 192.168.1.10/24
    gateway 192.168.1.1
    dns-nameservers 192.168.1.1 8.8.8.8
```

---

## 9. Diagnóstico del cliente DHCP

### Flujo de diagnóstico

```
¿El cliente obtiene IP por DHCP?
│
├── NO → ¿El servicio DHCP del cliente está activo?
│        │
│        ├── NO → Activar el gestor de red correspondiente
│        │
│        └── SÍ → ¿Se ve tráfico DHCP en la red?
│                 │
│                 ├── NO → Problema de capa 2 (cable, VLAN, switch)
│                 │        sudo tcpdump -i eth0 port 67 or port 68
│                 │
│                 └── SÍ → ¿Se ve DHCPOFFER del servidor?
│                          │
│                          ├── NO → El servidor no responde
│                          │        (verificar servidor, relay, firewall)
│                          │
│                          └── SÍ → ¿Se ve DHCPACK?
│                                   │
│                                   ├── NO → DHCPNAK (servidor rechaza)
│                                   │        Revisar configuración
│                                   │
│                                   └── SÍ → ¿Se aplica la IP?
│                                            Revisar hooks/scripts
│
└── SÍ pero con problemas → Ver sección siguiente
```

### Comandos de diagnóstico por gestor

```bash
# === Logs del cliente ===

# dhclient
grep dhclient /var/log/syslog
# o con systemd:
journalctl | grep dhclient

# NetworkManager
journalctl -u NetworkManager -f
nmcli general logging level DEBUG domains DHCP4
# (restaurar: nmcli general logging level INFO domains DEFAULT)

# systemd-networkd
journalctl -u systemd-networkd -f
# Aumentar verbosidad:
# En /etc/systemd/network.conf.d/debug.conf:
# [Network]
# SpeedMeter=yes
```

```bash
# === Captura de tráfico DHCP ===

# Ver todo el intercambio DHCP
sudo tcpdump -i eth0 -n -vv port 67 or port 68

# Filtrar solo por tipo de mensaje
sudo tcpdump -i eth0 -n port 67 or port 68 2>&1 | grep -E "Discover|Offer|Request|ACK|NAK"
```

```bash
# === Verificar configuración resultante ===

# IP asignada
ip addr show eth0

# Rutas (gateway del DHCP)
ip route show dev eth0

# DNS configurado
resolvectl status eth0    # Con systemd-resolved
cat /etc/resolv.conf      # Sin systemd-resolved
```

### Problemas frecuentes y soluciones

```bash
# Problema: "No DHCPOFFERS received"
# → El Discover no llega al servidor o el servidor no responde
sudo tcpdump -i eth0 -n port 67 or port 68 -c 10
# Verificar: ¿hay DISCOVER saliendo? ¿Hay OFFER volviendo?

# Problema: IP obtenida pero sin internet
# → Verificar gateway y DNS
ip route | grep default
ping -c 1 $(ip route | grep default | awk '{print $3}')
resolvectl query google.com

# Problema: IP duplicada (otro host tiene la misma IP)
# → El servidor asignó una IP que ya estaba en uso (estática)
arping -D -I eth0 192.168.1.100
# Si hay respuesta: conflicto de IP
# Solución: excluir esas IPs del pool DHCP en el servidor

# Problema: el cliente obtiene 169.254.x.x (APIPA)
# → No hay servidor DHCP accesible
# Verificar: servidor activo, conectividad L2, VLAN correcta
```

---

## 10. Errores comunes

### Error 1: Tener dos gestores de red activos simultáneamente

```
INCORRECTO:
# Ambos activos para la misma interfaz
$ systemctl is-active NetworkManager
active
$ systemctl is-active systemd-networkd
active

RESULTADO:
- Ambos intentan obtener un lease → IP duplicada en la interfaz
- Rutas que aparecen y desaparecen
- DNS que cambia inesperadamente

CORRECTO:
# Elegir UNO y desactivar el otro
sudo systemctl disable --now NetworkManager
sudo systemctl enable --now systemd-networkd

# O al revés:
sudo systemctl disable --now systemd-networkd
sudo systemctl enable --now NetworkManager

# En algunos sistemas, pueden coexistir si cada uno
# gestiona interfaces distintas (ej: NM para WiFi,
# systemd-networkd para eth0), pero requiere
# configuración explícita de exclusiones.
```

### Error 2: Confundir "renovar" con "obtener nueva IP"

```
INCORRECTO:
"Necesito otra IP, voy a renovar el lease"
$ sudo networkctl renew eth0
→ Sigue con la misma IP (el servidor extiende el lease existente)

CORRECTO:
La renovación EXTIENDE el lease actual con la MISMA IP.
Para obtener una IP diferente, hay que LIBERAR y luego obtener:

# dhclient:
sudo dhclient -r eth0 && sudo dhclient eth0

# NetworkManager:
nmcli device disconnect eth0 && nmcli device connect eth0

# systemd-networkd:
# No tiene release directo; cambiar MAC o esperar a que expire

# Nota: incluso liberando, el servidor puede reasignar
# la misma IP si no hay presión sobre el pool.
```

### Error 3: No adaptar dhclient.conf al cambiar de red

```
INCORRECTO:
# dhclient.conf con configuración de la oficina
supersede domain-name-servers 10.0.0.53;
supersede domain-search "corp.empresa.local";

# Se lleva el laptop a casa → sin conectividad DNS
# porque 10.0.0.53 no existe en la red doméstica

CORRECTO:
# Usar "default" en vez de "supersede" para valores condicionales
default domain-name-servers 8.8.8.8;
# Solo aplica si el servidor DHCP no envía DNS

# O usar "prepend" para añadir sin reemplazar
prepend domain-name-servers 10.0.0.53;
# Si 10.0.0.53 no responde, el siguiente DNS del DHCP funciona

# Mejor aún: usar NetworkManager con perfiles por red
# que aplican configuración automáticamente según la red.
```

### Error 4: Permisos incorrectos en scripts de dispatcher

```
INCORRECTO:
# Script creado pero no se ejecuta
$ ls -la /etc/NetworkManager/dispatcher.d/99-mi-script
-rw-r--r-- 1 usuario usuario 200 Mar 21 10:00 99-mi-script

CORRECTO:
# Los scripts del dispatcher DEBEN ser:
# 1. Propiedad de root:root
# 2. Ejecutables (755)
# 3. Sin extensión .conf o similar (deben ser ejecutables puros)

sudo chown root:root /etc/NetworkManager/dispatcher.d/99-mi-script
sudo chmod 755 /etc/NetworkManager/dispatcher.d/99-mi-script

# Verificar que se ejecuta:
journalctl -u NetworkManager-dispatcher -f
```

### Error 5: Asumir que systemd-networkd aplica cambios al editar el archivo

```
INCORRECTO:
# Editar el archivo .network y esperar que se aplique solo
sudo vim /etc/systemd/network/20-wired.network
# "Ya lo guardé, ¿por qué no cambia la IP?"

CORRECTO:
# Después de editar el archivo, hay que recargar:

# Opción 1: recargar y reconfigurar la interfaz específica
sudo networkctl reload
sudo networkctl reconfigure eth0

# Opción 2: reiniciar el servicio completo
sudo systemctl restart systemd-networkd

# networkctl reload solo relee los archivos
# networkctl reconfigure aplica los cambios a la interfaz
# Ambos son necesarios
```

---

## 11. Cheatsheet

```
DHCLIENT
──────────────────────────────────────────────
dhclient eth0               Obtener lease
dhclient -r eth0            Liberar lease
dhclient -v eth0            Obtener con verbose
dhclient -d eth0            Foreground + debug

Config:  /etc/dhcp/dhclient.conf
Leases:  /var/lib/dhcp/dhclient.*.leases
Hooks:   /etc/dhcp/dhclient-exit-hooks.d/

Directivas: supersede, prepend, append, default

NETWORKMANAGER
──────────────────────────────────────────────
nmcli con show                   Listar conexiones
nmcli con mod "X" ipv4.method auto   DHCP
nmcli con mod "X" ipv4.method manual IP estática
nmcli con up "X"                 Aplicar cambios
nmcli device reapply eth0        Renovar sin desconectar
nmcli -f DHCP4 device show eth0  Ver lease actual

Config:  /etc/NetworkManager/NetworkManager.conf
Hooks:   /etc/NetworkManager/dispatcher.d/

SYSTEMD-NETWORKD
──────────────────────────────────────────────
networkctl status              Estado general
networkctl status eth0         Estado interfaz
networkctl renew eth0          Renovar lease
networkctl reconfigure eth0    Reconfigurar interfaz
networkctl reload              Recargar archivos .network

Config:  /etc/systemd/network/*.network
Leases:  /run/systemd/netif/leases/*
Logs:    journalctl -u systemd-networkd

Secciones .network: [Match] [Network] [DHCPv4]

DIAGNÓSTICO GENERAL
──────────────────────────────────────────────
tcpdump -i eth0 port 67 or port 68    Tráfico DHCP
ip addr show eth0                      IP actual
ip route show dev eth0                 Rutas/gateway
resolvectl status eth0                 DNS actual

CUÁL ESTOY USANDO
──────────────────────────────────────────────
systemctl is-active NetworkManager
systemctl is-active systemd-networkd
ps aux | grep dhclient
```

---

## 12. Ejercicios

### Ejercicio 1: Identificar el cliente DHCP activo

En tu sistema actual:

1. Determina qué gestor de red está activo (NetworkManager, systemd-networkd, u otro).
2. Verifica si dhclient está corriendo como proceso separado.
3. Encuentra el archivo donde se almacena el lease actual y muestra su contenido.
4. Identifica: IP asignada, servidor DHCP, lease time, T1, T2, DNS recibidos.

```bash
# Pistas:
systemctl is-active NetworkManager
systemctl is-active systemd-networkd
ps aux | grep dhclient
# Luego busca el archivo de lease según el gestor detectado
```

### Ejercicio 2: Configurar opciones DHCP del cliente

Tienes un servidor interno con DNS local en 10.0.0.53. Quieres que tu cliente:
- Use 10.0.0.53 como DNS **principal** pero mantenga los DNS del DHCP como fallback.
- Envíe el hostname "dev-workstation" al servidor DHCP.
- No acepte el dominio de búsqueda que el servidor envía (quieres usar "dev.internal").

Escribe la configuración para los tres clientes:
1. `dhclient.conf`
2. Comandos `nmcli` para NetworkManager
3. Archivo `.network` para systemd-networkd

Verifica que cada configuración logra los tres objetivos.

### Ejercicio 3: Diagnosticar un cliente sin IP

Un compañero reporta que su máquina Ubuntu Server (con systemd-networkd) no obtiene IP. Los síntomas:
- `ip addr show eth0` muestra la interfaz UP pero sin dirección IPv4.
- `networkctl status eth0` muestra "configuring" (no llega a "configured").
- `journalctl -u systemd-networkd` muestra `eth0: DHCP: No DHCPOFFER received after 5 attempts`.

Describe tu proceso de diagnóstico paso a paso:
1. ¿Qué verificarías primero a nivel de red? (¿Cable? ¿Switch? ¿VLAN?)
2. ¿Qué comando usarías para ver si el DHCPDISCOVER sale realmente de la interfaz?
3. Si el Discover sale pero no hay Offer, ¿dónde está el problema?
4. Si hay un DHCP relay de por medio, ¿qué configuración verificarías?
5. ¿Cómo le darías una IP temporal para seguir trabajando mientras resuelves el DHCP?

**Pregunta de reflexión**: En un entorno con 500 servidores gestionados por systemd-networkd, todos usando DHCP con reserva, un colega propone cambiar todos a IP estática "para eliminar la dependencia del servidor DHCP." ¿Qué ventajas y desventajas tiene cada enfoque a esa escala? ¿Cuál elegirías y por qué?

---

> **Siguiente capítulo**: C04 — Configuración de Red en Linux (ip command, NetworkManager, systemd-networkd, /etc/network/interfaces)
