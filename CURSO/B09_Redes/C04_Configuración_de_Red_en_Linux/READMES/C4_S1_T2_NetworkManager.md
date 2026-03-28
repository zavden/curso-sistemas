# T02 — NetworkManager: nmcli, nmtui y Connection Profiles

## Índice

1. [¿Qué es NetworkManager?](#1-qué-es-networkmanager)
2. [Arquitectura](#2-arquitectura)
3. [nmcli — La herramienta de línea de comandos](#3-nmcli--la-herramienta-de-línea-de-comandos)
4. [Connection profiles](#4-connection-profiles)
5. [Configurar DHCP y IP estática](#5-configurar-dhcp-y-ip-estática)
6. [DNS y rutas](#6-dns-y-rutas)
7. [WiFi con nmcli](#7-wifi-con-nmcli)
8. [nmtui — Interfaz de texto](#8-nmtui--interfaz-de-texto)
9. [Archivos de configuración](#9-archivos-de-configuración)
10. [Plugins y backends](#10-plugins-y-backends)
11. [Convivencia con otros gestores](#11-convivencia-con-otros-gestores)
12. [Diferencias entre distribuciones](#12-diferencias-entre-distribuciones)
13. [Errores comunes](#13-errores-comunes)
14. [Cheatsheet](#14-cheatsheet)
15. [Ejercicios](#15-ejercicios)

---

## 1. ¿Qué es NetworkManager?

NetworkManager es un daemon que gestiona automáticamente las conexiones de red en Linux. Su propósito es que la red "simplemente funcione": detecta interfaces, aplica configuración, gestiona DHCP, resuelve conflictos entre redes, y maneja transiciones (cable → WiFi, VPN arriba/abajo).

```
Sin NetworkManager:                 Con NetworkManager:
┌────────────────────┐              ┌──────────────────────┐
│ Editar archivos    │              │ NetworkManager       │
│ de config          │              │                      │
│ Reiniciar servicio │              │ Detecta interfaces   │
│ Verificar estado   │              │ Aplica config auto   │
│ Debug manual       │              │ DHCP integrado       │
│                    │              │ Gestiona WiFi        │
│ Por cada cambio:   │              │ Soporta VPN          │
│ repetir todo       │              │ Dispatcher hooks     │
└────────────────────┘              │ GUI / TUI / CLI      │
                                    └──────────────────────┘
```

NetworkManager no es solo para escritorios. Se usa también en servidores RHEL/CentOS/Fedora como gestor principal de red. La diferencia está en cómo interactúas con él:

| Entorno | Herramienta |
|---------|-------------|
| Escritorio GNOME | nm-applet (icono en la barra), Settings |
| Escritorio KDE | plasma-nm |
| Terminal (servidores) | `nmcli` |
| Terminal (interactivo) | `nmtui` |
| Programático | D-Bus API, libnm |

---

## 2. Arquitectura

```
┌─────────────────────────────────────────────────────────┐
│                     Aplicaciones                        │
│  nm-applet   GNOME Settings   nmcli   nmtui   scripts  │
└──────────┬──────────┬──────────┬────────┬───────┬───────┘
           │          │          │        │       │
           ▼          ▼          ▼        ▼       ▼
┌─────────────────────────────────────────────────────────┐
│                    D-Bus (IPC)                          │
│           org.freedesktop.NetworkManager                │
└────────────────────────┬────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────┐
│              NetworkManager daemon                      │
│                                                         │
│  ┌──────────┐  ┌──────────┐  ┌───────────┐             │
│  │  DHCP    │  │  DNS     │  │  VPN      │             │
│  │ (interno │  │ (plugin) │  │ (plugins) │             │
│  │  o ext.) │  │          │  │           │             │
│  └──────────┘  └──────────┘  └───────────┘             │
│                                                         │
│  ┌──────────────────────────────────────────────┐       │
│  │        Settings plugins                      │       │
│  │   keyfile   ifcfg-rh   ifupdown              │       │
│  └──────────────────────────────────────────────┘       │
└────────────────────────┬────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────┐
│                    Kernel Linux                         │
│          Netlink / nl80211 / wpa_supplicant             │
└─────────────────────────────────────────────────────────┘
```

Componentes clave:

- **Daemon** (`NetworkManager`): el proceso principal, corre como root
- **D-Bus**: bus de comunicación entre el daemon y las herramientas (nmcli, GUI, etc.)
- **Connection profiles**: archivos que definen configuraciones de red
- **Settings plugins**: cómo se almacenan los profiles en disco (keyfile, ifcfg-rh)
- **Dispatcher**: scripts que se ejecutan ante eventos de red

---

## 3. nmcli — La herramienta de línea de comandos

`nmcli` es la herramienta principal para controlar NetworkManager desde la terminal. Tiene una estructura consistente:

```
nmcli [OPTIONS] OBJECT {COMMAND}

Objetos principales:
  general       Estado general de NetworkManager
  networking    Control on/off de toda la red
  connection    Gestionar connection profiles
  device        Gestionar interfaces de red
  radio         Control de WiFi/WWAN
  monitor       Observar eventos
```

### 3.1 Estado general

```bash
# Estado de NetworkManager
nmcli general status
```

```
STATE      CONNECTIVITY  WIFI-HW  WIFI     WWAN-HW  WWAN
connected  full          enabled  enabled  enabled  enabled
```

```bash
# ¿Está NetworkManager corriendo?
nmcli general
# O simplemente:
nmcli

# Hostname del sistema
nmcli general hostname
nmcli general hostname nuevo-hostname   # Cambiar hostname
```

### 3.2 Dispositivos (interfaces)

Los **devices** son las interfaces físicas y virtuales del sistema:

```bash
# Listar dispositivos
nmcli device status
```

```
DEVICE  TYPE      STATE                  CONNECTION
eth0    ethernet  connected              Wired connection 1
wlan0   wifi      disconnected           --
lo      loopback  connected (externally) lo
```

```bash
# Detalles de un dispositivo
nmcli device show eth0
```

```
GENERAL.DEVICE:                         eth0
GENERAL.TYPE:                           ethernet
GENERAL.HWADDR:                         AA:BB:CC:11:22:33
GENERAL.MTU:                            1500
GENERAL.STATE:                          100 (connected)
GENERAL.CONNECTION:                     Wired connection 1
GENERAL.CON-PATH:                       /org/freedesktop/NetworkManager/ActiveConnection/1
WIRED-PROPERTIES.CARRIER:               on
IP4.ADDRESS[1]:                         192.168.1.100/24
IP4.GATEWAY:                            192.168.1.1
IP4.ROUTE[1]:                           dst = 0.0.0.0/0, nh = 192.168.1.1, mt = 100
IP4.ROUTE[2]:                           dst = 192.168.1.0/24, nh = 0.0.0.0, mt = 100
IP4.DNS[1]:                             192.168.1.1
IP4.DNS[2]:                             8.8.8.8
IP4.DOMAIN[1]:                          home.local
DHCP4.OPTION[1]:                        dhcp-lease-time = 28800
DHCP4.OPTION[2]:                        dhcp-server-identifier = 192.168.1.1
...
```

```bash
# Conectar/desconectar un dispositivo
nmcli device connect eth0
nmcli device disconnect eth0

# Reaplicar configuración sin desconectar
nmcli device reapply eth0

# Escanear redes WiFi
nmcli device wifi list
nmcli device wifi rescan
```

### 3.3 Conexiones (profiles)

Las **connections** son perfiles de configuración. Un dispositivo puede tener múltiples conexiones asociadas, pero solo una activa a la vez:

```
Relación device ↔ connection:

device (interfaz)           connections (profiles)
┌──────────┐                ┌─────────────────────────┐
│  eth0    │◄───── activa ──│ "Wired connection 1"    │  DHCP
│          │                ├─────────────────────────┤
│          │    inactiva ──►│ "Office Static"         │  IP fija oficina
│          │                ├─────────────────────────┤
│          │    inactiva ──►│ "Lab Network"           │  IP fija lab
│          │                └─────────────────────────┘
└──────────┘
Se puede activar cualquiera según el contexto
```

```bash
# Listar conexiones
nmcli connection show
```

```
NAME                UUID                                  TYPE      DEVICE
Wired connection 1  a1b2c3d4-e5f6-7890-abcd-ef1234567890  ethernet  eth0
Home WiFi           b2c3d4e5-f6a7-8901-bcde-f12345678901  wifi      --
VPN Office          c3d4e5f6-a7b8-9012-cdef-123456789012  vpn       --
```

```bash
# Detalles de una conexión
nmcli connection show "Wired connection 1"

# Solo conexiones activas
nmcli connection show --active

# Activar una conexión
nmcli connection up "Office Static"

# Desactivar
nmcli connection down "Wired connection 1"

# Eliminar
nmcli connection delete "Lab Network"
```

### 3.4 Crear conexiones

```bash
# Crear conexión ethernet con DHCP
nmcli connection add type ethernet con-name "DHCP-eth0" ifname eth0

# Crear conexión ethernet con IP estática
nmcli connection add type ethernet con-name "Static-eth0" ifname eth0 \
    ipv4.method manual \
    ipv4.addresses "192.168.1.50/24" \
    ipv4.gateway "192.168.1.1" \
    ipv4.dns "192.168.1.1,8.8.8.8"

# Crear conexión WiFi
nmcli connection add type wifi con-name "Mi-WiFi" \
    wifi.ssid "NombreRed" \
    wifi-sec.key-mgmt wpa-psk \
    wifi-sec.psk "contraseña"

# Crear conexión con autoconnect deshabilitado
nmcli connection add type ethernet con-name "Manual" ifname eth0 \
    connection.autoconnect no
```

### 3.5 Modificar conexiones

```bash
# Cambiar de DHCP a estática
nmcli connection modify "Wired connection 1" \
    ipv4.method manual \
    ipv4.addresses "192.168.1.50/24" \
    ipv4.gateway "192.168.1.1" \
    ipv4.dns "8.8.8.8"

# Volver a DHCP
nmcli connection modify "Wired connection 1" \
    ipv4.method auto \
    ipv4.addresses "" \
    ipv4.gateway "" \
    ipv4.dns ""

# Agregar DNS sin reemplazar (+ipv4.dns)
nmcli connection modify "Wired connection 1" +ipv4.dns "1.1.1.1"

# Eliminar un DNS específico (-ipv4.dns)
nmcli connection modify "Wired connection 1" -ipv4.dns "1.1.1.1"

# Agregar ruta estática
nmcli connection modify "Wired connection 1" \
    +ipv4.routes "10.0.0.0/8 192.168.1.254"

# Cambiar el MTU
nmcli connection modify "Wired connection 1" 802-3-ethernet.mtu 9000

# Deshabilitar autoconnect
nmcli connection modify "Wired connection 1" connection.autoconnect no

# APLICAR CAMBIOS (modify solo cambia el archivo, no la conexión activa)
nmcli connection up "Wired connection 1"
# O sin desconectar:
nmcli device reapply eth0
```

> **Importante**: `nmcli connection modify` modifica el **perfil guardado en disco**, no la conexión activa. Debes hacer `connection up` o `device reapply` para aplicar los cambios.

### 3.6 Operadores + y - para listas

```bash
# El prefijo + AÑADE a una lista existente
nmcli connection modify "X" +ipv4.dns "1.1.1.1"
nmcli connection modify "X" +ipv4.addresses "10.0.0.5/24"
nmcli connection modify "X" +ipv4.routes "172.16.0.0/12 192.168.1.254"

# El prefijo - ELIMINA de la lista
nmcli connection modify "X" -ipv4.dns "1.1.1.1"
nmcli connection modify "X" -ipv4.addresses "10.0.0.5/24"

# Sin prefijo REEMPLAZA toda la lista
nmcli connection modify "X" ipv4.dns "8.8.8.8"
# Ahora solo hay un DNS, los anteriores se borraron
```

---

## 4. Connection profiles

Un connection profile contiene toda la configuración necesaria para establecer una conexión de red. Incluye:

```
Connection Profile:
┌──────────────────────────────────────────────┐
│ [connection]                                 │
│   id, uuid, type, interface-name             │
│   autoconnect, autoconnect-priority          │
│                                              │
│ [ipv4]                                       │
│   method (auto/manual/disabled)              │
│   addresses, gateway, dns, routes            │
│   dhcp-hostname, dhcp-client-id              │
│   never-default, ignore-auto-dns             │
│                                              │
│ [ipv6]                                       │
│   method, addresses, gateway, dns            │
│                                              │
│ [ethernet] / [wifi] / [vpn]                  │
│   Propiedades específicas del tipo           │
│                                              │
│ [802-3-ethernet]                             │
│   mtu, mac-address, cloned-mac-address       │
│                                              │
│ [wifi-security]                              │
│   key-mgmt, psk                              │
└──────────────────────────────────────────────┘
```

### Prioridad de autoconnect

Cuando un dispositivo se activa, NetworkManager busca el perfil adecuado. Si hay varios candidatos, usa la prioridad:

```bash
# Ver prioridades
nmcli -f NAME,AUTOCONNECT,AUTOCONNECT-PRIORITY connection show

# Cambiar prioridad (mayor número = mayor prioridad)
nmcli connection modify "Office Static" connection.autoconnect-priority 10
nmcli connection modify "DHCP-eth0" connection.autoconnect-priority 5

# "Office Static" se activará primero si ambas coinciden
```

### Restricción de perfil a una interfaz o MAC

```bash
# Vincular perfil a una interfaz específica
nmcli connection modify "X" connection.interface-name eth0

# Vincular a una MAC (perfil se activa solo en esa NIC)
nmcli connection modify "X" 802-3-ethernet.mac-address "AA:BB:CC:11:22:33"

# Perfil genérico (funciona en cualquier ethernet)
nmcli connection modify "X" connection.interface-name ""
```

---

## 5. Configurar DHCP y IP estática

### DHCP (método auto)

```bash
# Configurar DHCP (lo más simple)
nmcli connection modify "eth0" ipv4.method auto

# DHCP con hostname personalizado
nmcli connection modify "eth0" \
    ipv4.method auto \
    ipv4.dhcp-hostname "mi-servidor"

# DHCP pero ignorar el DNS del servidor
nmcli connection modify "eth0" \
    ipv4.method auto \
    ipv4.ignore-auto-dns yes \
    ipv4.dns "1.1.1.1,8.8.8.8"

# DHCP pero ignorar las rutas del servidor
nmcli connection modify "eth0" \
    ipv4.method auto \
    ipv4.ignore-auto-routes yes

# DHCP sin establecer ruta por defecto (útil para interfaces secundarias)
nmcli connection modify "eth0" \
    ipv4.method auto \
    ipv4.never-default yes

# Aplicar
nmcli connection up "eth0"
```

### IP estática (método manual)

```bash
# IP estática completa
nmcli connection modify "eth0" \
    ipv4.method manual \
    ipv4.addresses "192.168.1.50/24" \
    ipv4.gateway "192.168.1.1" \
    ipv4.dns "192.168.1.1,8.8.8.8" \
    ipv4.dns-search "empresa.local"

# Múltiples IPs
nmcli connection modify "eth0" \
    ipv4.method manual \
    ipv4.addresses "192.168.1.50/24,10.0.0.1/24"

# Aplicar
nmcli connection up "eth0"
```

### IPv6

```bash
# DHCPv6
nmcli connection modify "eth0" ipv6.method auto

# SLAAC solamente (sin DHCPv6)
nmcli connection modify "eth0" ipv6.method auto \
    ipv6.ip6-privacy 2    # Privacy extensions (IPs temporales)

# IPv6 estática
nmcli connection modify "eth0" \
    ipv6.method manual \
    ipv6.addresses "2001:db8::1/64" \
    ipv6.gateway "2001:db8::ffff"

# Deshabilitar IPv6
nmcli connection modify "eth0" ipv6.method disabled

# Link-local only (sin direcciones globales)
nmcli connection modify "eth0" ipv6.method link-local
```

---

## 6. DNS y rutas

### Gestión de DNS

```bash
# Ver DNS actual
nmcli device show eth0 | grep DNS

# Configurar DNS estáticos (reemplaza)
nmcli connection modify "eth0" ipv4.dns "1.1.1.1 8.8.8.8"

# Agregar DNS sin borrar los existentes
nmcli connection modify "eth0" +ipv4.dns "9.9.9.9"

# Dominio de búsqueda
nmcli connection modify "eth0" ipv4.dns-search "empresa.local,dev.internal"

# Prioridad DNS (número menor = mayor prioridad)
# Negativo: este DNS se consulta ANTES que otros
nmcli connection modify "eth0" ipv4.dns-priority -100

# Ignorar DNS del DHCP y usar solo los configurados
nmcli connection modify "eth0" ipv4.ignore-auto-dns yes
```

**Interacción con systemd-resolved**:

```
NetworkManager + systemd-resolved:

NM obtiene DNS del DHCP (o config estática)
    │
    ├── dns=default (en NM.conf)
    │   NM pasa los DNS a resolved vía D-Bus
    │   resolved resuelve las consultas
    │
    ├── dns=systemd-resolved
    │   Igual que default, pero NM actualiza /etc/resolv.conf
    │   para apuntar a 127.0.0.53
    │
    └── dns=none
        NM no toca /etc/resolv.conf
        El usuario lo gestiona manualmente
```

```ini
# /etc/NetworkManager/NetworkManager.conf
[main]
dns=systemd-resolved    # Recomendado con resolved activo
```

### Rutas estáticas

```bash
# Agregar ruta estática
nmcli connection modify "eth0" +ipv4.routes "10.0.0.0/8 192.168.1.254"

# Ruta con metric
nmcli connection modify "eth0" +ipv4.routes "10.0.0.0/8 192.168.1.254 200"

# Múltiples rutas
nmcli connection modify "eth0" \
    ipv4.routes "10.0.0.0/8 192.168.1.254, 172.16.0.0/12 192.168.1.253"

# Eliminar una ruta
nmcli connection modify "eth0" -ipv4.routes "10.0.0.0/8 192.168.1.254"

# No establecer ruta por defecto (para interfaces secundarias)
nmcli connection modify "eth0" ipv4.never-default yes

# Metric de la ruta por defecto
nmcli connection modify "eth0" ipv4.route-metric 200
```

---

## 7. WiFi con nmcli

```bash
# Ver redes WiFi disponibles
nmcli device wifi list
```

```
IN-USE  BSSID              SSID            MODE   CHAN  RATE       SIGNAL  BARS  SECURITY
        AA:BB:CC:DD:EE:FF  HomeNetwork     Infra  6    270 Mbit/s  85      ▂▄▆█  WPA2
        11:22:33:44:55:66  OfficeWiFi      Infra  36   540 Mbit/s  72      ▂▄▆_  WPA2 802.1X
*       77:88:99:AA:BB:CC  CafeLibre       Infra  1    130 Mbit/s  95      ▂▄▆█  WPA2
```

```bash
# Conectar a red WPA2-PSK
nmcli device wifi connect "HomeNetwork" password "mi_contraseña"

# Conectar especificando interfaz
nmcli device wifi connect "HomeNetwork" password "clave" ifname wlan0

# Conectar a red oculta
nmcli device wifi connect "RedOculta" password "clave" hidden yes

# Desconectar WiFi
nmcli device disconnect wlan0

# Activar/desactivar WiFi
nmcli radio wifi on
nmcli radio wifi off

# Forzar re-escaneo
nmcli device wifi rescan
```

```bash
# Crear perfil WiFi sin conectar inmediatamente
nmcli connection add type wifi con-name "Trabajo" \
    wifi.ssid "OfficeWiFi" \
    wifi-sec.key-mgmt wpa-psk \
    wifi-sec.psk "contraseña_corporativa" \
    connection.autoconnect yes \
    connection.autoconnect-priority 20

# WPA2-Enterprise (802.1X)
nmcli connection add type wifi con-name "Enterprise" \
    wifi.ssid "CorpNet" \
    wifi-sec.key-mgmt wpa-eap \
    802-1x.eap peap \
    802-1x.phase2-auth mschapv2 \
    802-1x.identity "usuario" \
    802-1x.password "contraseña"
```

### Prioridad de redes WiFi

```bash
# Ver prioridades
nmcli -f NAME,AUTOCONNECT-PRIORITY connection show | grep wifi

# Red de trabajo primero (prioridad alta)
nmcli connection modify "Trabajo" connection.autoconnect-priority 100

# Red de casa segundo
nmcli connection modify "HomeNetwork" connection.autoconnect-priority 50

# Red de café último
nmcli connection modify "CafeLibre" connection.autoconnect-priority 10
```

---

## 8. nmtui — Interfaz de texto

`nmtui` proporciona una interfaz ncurses (texto interactivo) para configurar NetworkManager. Ideal cuando necesitas una interfaz visual pero no tienes escritorio:

```bash
# Lanzar nmtui
sudo nmtui
```

```
┌─────────────────────────────────────────┐
│         NetworkManager TUI              │
│                                         │
│    ┌─────────────────────────────────┐  │
│    │  Edit a connection              │  │
│    │  Activate a connection          │  │
│    │  Set system hostname            │  │
│    │                                 │  │
│    │  Quit                           │  │
│    └─────────────────────────────────┘  │
│                                         │
└─────────────────────────────────────────┘
```

```bash
# Ir directamente a una función específica
nmtui edit              # Editar conexiones
nmtui connect           # Activar/desactivar conexiones
nmtui hostname          # Cambiar hostname
```

Dentro de "Edit a connection", puedes configurar visualmente:

```
┌────────────────────────────────────────────────┐
│ Editing Wired connection 1                     │
│                                                │
│ Profile name: Wired connection 1___            │
│ Device:       eth0_____                        │
│                                                │
│ ═ ETHERNET                                     │
│   MTU: ________                                │
│                                                │
│ ═ IPv4 CONFIGURATION  <Automatic>     ▾        │
│   Addresses  ┌──────────────────────┐          │
│              │ <Add...>             │          │
│              └──────────────────────┘          │
│   Gateway    _______________                   │
│   DNS servers┌──────────────────────┐          │
│              │ <Add...>             │          │
│              └──────────────────────┘          │
│   Search dom ┌──────────────────────┐          │
│              │ <Add...>             │          │
│              └──────────────────────┘          │
│                                                │
│ [X] Automatically connect                      │
│ [ ] Available to all users                     │
│                                                │
│              <Cancel>     <OK>                  │
└────────────────────────────────────────────────┘
```

`nmtui` es especialmente útil para:
- Configuración rápida de IP estática en servidores nuevos
- Usuarios que no recuerdan la sintaxis de `nmcli`
- Sesiones SSH donde se necesita modificar la red con cuidado

> **Precaución**: si estás conectado por SSH y cambias la IP de la interfaz por la que estás conectado, perderás la conexión. `nmtui` no te advierte de esto.

---

## 9. Archivos de configuración

### NetworkManager.conf

```ini
# /etc/NetworkManager/NetworkManager.conf
# Archivo principal de configuración

[main]
# Plugin de almacenamiento de perfiles
plugins=keyfile
# En RHEL/CentOS: plugins=keyfile,ifcfg-rh

# Backend DHCP
dhcp=internal
# Opciones: internal (default), dhclient

# Gestión de DNS
dns=systemd-resolved
# Opciones: default, systemd-resolved, dnsmasq, none

# No gestionar ciertos dispositivos
# (ver sección "Convivencia")

[logging]
level=INFO
# Opciones: ERR, WARN, INFO, DEBUG, TRACE

[connectivity]
# Verificar conectividad a Internet
uri=http://nmcheck.gnome.org/check_network_status.txt
interval=300
```

```bash
# Drop-in configs (se aplican sobre el principal)
/etc/NetworkManager/conf.d/*.conf

# Ejemplo: /etc/NetworkManager/conf.d/99-dns.conf
# [main]
# dns=systemd-resolved
```

### Connection profiles en disco — formato keyfile

El formato **keyfile** es el formato nativo y moderno de NetworkManager:

```bash
# Ubicación de profiles
/etc/NetworkManager/system-connections/

# Listar profiles
ls /etc/NetworkManager/system-connections/
```

```ini
# /etc/NetworkManager/system-connections/Wired-connection-1.nmconnection

[connection]
id=Wired connection 1
uuid=a1b2c3d4-e5f6-7890-abcd-ef1234567890
type=ethernet
autoconnect=true
interface-name=eth0

[ethernet]
mac-address=AA:BB:CC:11:22:33

[ipv4]
method=auto

[ipv6]
method=auto

[proxy]
```

```ini
# Ejemplo: conexión con IP estática
# /etc/NetworkManager/system-connections/Static-eth0.nmconnection

[connection]
id=Static-eth0
uuid=b2c3d4e5-f6a7-8901-bcde-f12345678901
type=ethernet
interface-name=eth0

[ipv4]
method=manual
address1=192.168.1.50/24,192.168.1.1
dns=192.168.1.1;8.8.8.8;
dns-search=empresa.local;

[ipv6]
method=disabled
```

```bash
# Los perfiles deben tener permisos restrictivos
ls -la /etc/NetworkManager/system-connections/
# -rw------- 1 root root ... Wired-connection-1.nmconnection

# Después de editar manualmente un archivo, recargar:
sudo nmcli connection reload
```

> **Importante**: el formato del archivo keyfile difiere de la sintaxis de `nmcli`. Por ejemplo, en el archivo la dirección y gateway van juntos (`address1=192.168.1.50/24,192.168.1.1`), mientras que en nmcli son propiedades separadas.

### Formato ifcfg-rh (RHEL/CentOS legacy)

En RHEL/CentOS 7 y 8, los profiles se almacenaban en formato `ifcfg-*`:

```bash
# Ubicación
/etc/sysconfig/network-scripts/ifcfg-*
```

```ini
# /etc/sysconfig/network-scripts/ifcfg-eth0
TYPE=Ethernet
BOOTPROTO=dhcp
DEFROUTE=yes
NAME=eth0
DEVICE=eth0
ONBOOT=yes

# O con IP estática:
BOOTPROTO=none
IPADDR=192.168.1.50
PREFIX=24
GATEWAY=192.168.1.1
DNS1=192.168.1.1
DNS2=8.8.8.8
```

> **Nota**: RHEL 9+ usa exclusivamente keyfile. El formato ifcfg-rh está deprecated. Si migras de RHEL 8 a 9, los perfiles ifcfg se convierten automáticamente a keyfile con `nmcli connection migrate`.

---

## 10. Plugins y backends

### Plugin de DNS

NetworkManager puede gestionar el DNS de varias formas:

```ini
# /etc/NetworkManager/NetworkManager.conf
[main]
dns=default           # NM actualiza /etc/resolv.conf directamente
dns=systemd-resolved  # NM pasa DNS a resolved (recomendado con resolved)
dns=dnsmasq           # NM levanta dnsmasq como cache local
dns=none              # NM no toca DNS (lo gestionas tú)
```

```
dns=default:
  NM ──► escribe /etc/resolv.conf directamente

dns=systemd-resolved:
  NM ──► D-Bus ──► systemd-resolved ──► 127.0.0.53
                                         (/etc/resolv.conf apunta aquí)

dns=dnsmasq:
  NM ──► levanta dnsmasq en 127.0.0.1
         /etc/resolv.conf → nameserver 127.0.0.1
         dnsmasq → upstream DNS del DHCP

dns=none:
  NM ──► no toca nada
         Tú gestionas /etc/resolv.conf
```

### Plugin de VPN

```bash
# Plugins VPN comunes
NetworkManager-openvpn         # OpenVPN
NetworkManager-vpnc            # Cisco VPN
NetworkManager-openconnect     # Cisco AnyConnect compatible
NetworkManager-wireguard       # WireGuard
NetworkManager-l2tp            # L2TP/IPSec
NetworkManager-strongswan      # IPSec/IKEv2

# Instalar (Fedora/RHEL)
sudo dnf install NetworkManager-openvpn

# Instalar (Ubuntu/Debian)
sudo apt install network-manager-openvpn

# Importar configuración OpenVPN
nmcli connection import type openvpn file client.ovpn
```

---

## 11. Convivencia con otros gestores

### Excluir interfaces de NetworkManager

Cuando quieres que NetworkManager **no toque** ciertas interfaces (porque las gestiona systemd-networkd, un contenedor, o manualmente):

```ini
# /etc/NetworkManager/conf.d/99-unmanaged.conf

[keyfile]
# No gestionar interfaces específicas
unmanaged-devices=interface-name:eth1;interface-name:docker0

# Por MAC
unmanaged-devices=mac:AA:BB:CC:DD:EE:FF

# Por tipo
unmanaged-devices=type:bridge

# Por patrón
unmanaged-devices=interface-name:veth*;interface-name:br-*;interface-name:docker*
```

```bash
# Verificar que la interfaz no está gestionada
nmcli device status
# eth1    ethernet  unmanaged  --

# O temporalmente:
nmcli device set eth1 managed no
```

### NetworkManager + Docker

Docker crea interfaces (bridges, veth pairs) que NetworkManager no debe gestionar:

```ini
# /etc/NetworkManager/conf.d/99-docker.conf
[keyfile]
unmanaged-devices=interface-name:docker0;interface-name:br-*;interface-name:veth*
```

### NetworkManager + systemd-networkd

Pueden coexistir si cada uno gestiona interfaces distintas:

```
NetworkManager gestiona:        systemd-networkd gestiona:
├── wlan0 (WiFi)                ├── eth0 (servidor)
├── VPN connections             └── eth1 (red interna)
└── USB tethering
```

```ini
# NM no toca eth0 y eth1
# /etc/NetworkManager/conf.d/99-unmanaged.conf
[keyfile]
unmanaged-devices=interface-name:eth0;interface-name:eth1
```

---

## 12. Diferencias entre distribuciones

```
┌─────────────────┬───────────────────────────────────────────────┐
│ Distribución    │ Situación de NetworkManager                   │
├─────────────────┼───────────────────────────────────────────────┤
│ Fedora          │ NM siempre, keyfile, systemd-resolved         │
│                 │ Referencia: NM se desarrolla aquí             │
├─────────────────┼───────────────────────────────────────────────┤
│ RHEL 9 / CentOS │ NM siempre, keyfile (migrado de ifcfg-rh)    │
│ Stream 9        │ nmcli connection migrate convierte            │
├─────────────────┼───────────────────────────────────────────────┤
│ RHEL 8 / CentOS │ NM con ifcfg-rh (legacy)                     │
│ Stream 8        │ Archivos en /etc/sysconfig/network-scripts/   │
├─────────────────┼───────────────────────────────────────────────┤
│ Ubuntu Desktop  │ NM, gestionado por netplan (renderer: NM)     │
│                 │ YAML en /etc/netplan/ → genera config NM      │
├─────────────────┼───────────────────────────────────────────────┤
│ Ubuntu Server   │ netplan con renderer: networkd (no NM)        │
│                 │ NM no instalado por defecto                   │
├─────────────────┼───────────────────────────────────────────────┤
│ Debian          │ NM disponible pero no siempre default         │
│                 │ Coexiste con ifupdown (/etc/network/interfaces)│
├─────────────────┼───────────────────────────────────────────────┤
│ Arch Linux      │ NM disponible, no default                     │
│                 │ El usuario elige NM, systemd-networkd, u otro │
└─────────────────┴───────────────────────────────────────────────┘
```

### Ubuntu y Netplan

En Ubuntu, NetworkManager no se configura directamente. En su lugar, **netplan** actúa como capa de abstracción:

```yaml
# /etc/netplan/01-network-config.yaml
network:
  version: 2
  renderer: NetworkManager    # Desktop
  # renderer: networkd        # Server (systemd-networkd)
  ethernets:
    eth0:
      dhcp4: true
  wifis:
    wlan0:
      dhcp4: true
      access-points:
        "HomeNetwork":
          password: "mi_clave"
```

```bash
# Aplicar configuración netplan
sudo netplan apply

# Ver configuración generada
sudo netplan get

# Debug
sudo netplan try   # Aplica temporalmente, revierte si no confirmas
```

Cuando netplan usa `renderer: NetworkManager`, genera perfiles NM automáticamente. Puedes usar `nmcli` normalmente después.

---

## 13. Errores comunes

### Error 1: Modificar el perfil y olvidar aplicar los cambios

```
INCORRECTO:
nmcli connection modify "eth0" ipv4.dns "1.1.1.1"
# "Ya lo cambié" → pero la conexión activa sigue con el DNS anterior

CORRECTO:
nmcli connection modify "eth0" ipv4.dns "1.1.1.1"
nmcli connection up "eth0"         # Reactivar (breve interrupción)
# O mejor:
nmcli device reapply eth0          # Sin interrupción (cuando es posible)

# Verificar que se aplicó:
nmcli device show eth0 | grep DNS
```

### Error 2: Usar ipv4.dns sin limpiar cuando se cambia de DHCP a manual

```
INCORRECTO:
# Estaba en DHCP, cambio a manual
nmcli connection modify "eth0" ipv4.method manual \
    ipv4.addresses "192.168.1.50/24" \
    ipv4.gateway "192.168.1.1"
# Olvidé configurar DNS → sin resolución de nombres

CORRECTO:
nmcli connection modify "eth0" \
    ipv4.method manual \
    ipv4.addresses "192.168.1.50/24" \
    ipv4.gateway "192.168.1.1" \
    ipv4.dns "192.168.1.1,8.8.8.8"     # ← No olvidar DNS

# En modo auto (DHCP), el DNS viene del servidor.
# En modo manual, debes especificarlo explícitamente.
```

### Error 3: Confundir device y connection

```
INCORRECTO:
# "Quiero cambiar la IP de eth0"
nmcli device modify eth0 ipv4.addresses "192.168.1.50/24"
# Esto modifica la configuración EN MEMORIA del dispositivo,
# no persiste al reiniciar

CORRECTO:
# Modificar el PERFIL (connection) para que persista
nmcli connection modify "Wired connection 1" \
    ipv4.method manual \
    ipv4.addresses "192.168.1.50/24"
nmcli connection up "Wired connection 1"

# device = interfaz física (eth0)
# connection = perfil de configuración ("Wired connection 1")
# Modifica la connection, no el device
```

### Error 4: Permisos del archivo .nmconnection

```
INCORRECTO:
# Crear archivo manualmente con permisos incorrectos
sudo vim /etc/NetworkManager/system-connections/mi-red.nmconnection
# El archivo queda con permisos 644 → NM lo ignora o da error
# (las contraseñas WiFi quedarían legibles para todos)

CORRECTO:
# Los archivos deben ser modo 600 propiedad de root
sudo chmod 600 /etc/NetworkManager/system-connections/mi-red.nmconnection
sudo chown root:root /etc/NetworkManager/system-connections/mi-red.nmconnection
sudo nmcli connection reload

# Mejor aún: usar nmcli para crear perfiles, maneja permisos automáticamente
nmcli connection add type wifi con-name "mi-red" ...
```

### Error 5: No entender que nmcli connection modify cambia ipv4.addresses, no la añade

```
INCORRECTO:
# Ya tengo 192.168.1.50/24 y quiero agregar 10.0.0.1/24
nmcli connection modify "eth0" ipv4.addresses "10.0.0.1/24"
# Resultado: solo tiene 10.0.0.1/24, perdió 192.168.1.50/24

CORRECTO:
# Usar + para AGREGAR sin reemplazar
nmcli connection modify "eth0" +ipv4.addresses "10.0.0.1/24"
# Resultado: tiene ambas IPs

# Lo mismo aplica a DNS y rutas:
nmcli connection modify "eth0" +ipv4.dns "1.1.1.1"
nmcli connection modify "eth0" +ipv4.routes "10.0.0.0/8 192.168.1.254"
```

---

## 14. Cheatsheet

```
ESTADO Y DIAGNÓSTICO
──────────────────────────────────────────────
nmcli                             Estado general
nmcli device status               Interfaces y estado
nmcli device show eth0             Detalles de interfaz
nmcli connection show              Listar perfiles
nmcli connection show --active     Solo activos
nmcli connection show "X"          Detalles de perfil

CONEXIONES
──────────────────────────────────────────────
nmcli con add type ethernet con-name "X" ifname eth0
nmcli con modify "X" PROP VALUE   Modificar perfil
nmcli con up "X"                  Activar perfil
nmcli con down "X"                Desactivar
nmcli con delete "X"              Eliminar perfil
nmcli con reload                  Recargar archivos

OPERADOR +/- PARA LISTAS
──────────────────────────────────────────────
+ipv4.dns "1.1.1.1"     Agregar DNS
-ipv4.dns "1.1.1.1"     Eliminar DNS
 ipv4.dns "1.1.1.1"     Reemplazar todos los DNS
(igual para addresses, routes)

DHCP ↔ ESTÁTICA
──────────────────────────────────────────────
ipv4.method auto                  DHCP
ipv4.method manual                IP estática
  ipv4.addresses "IP/M"
  ipv4.gateway "GW"
  ipv4.dns "DNS1,DNS2"

WIFI
──────────────────────────────────────────────
nmcli device wifi list            Escanear redes
nmcli device wifi connect "SSID" password "PSK"
nmcli radio wifi on/off           Encender/apagar WiFi

DISPOSITIVOS
──────────────────────────────────────────────
nmcli device connect eth0         Conectar
nmcli device disconnect eth0      Desconectar
nmcli device reapply eth0         Reaplicar config

ARCHIVOS
──────────────────────────────────────────────
/etc/NetworkManager/NetworkManager.conf     Config principal
/etc/NetworkManager/conf.d/*.conf           Drop-ins
/etc/NetworkManager/system-connections/     Perfiles (keyfile)
/etc/NetworkManager/dispatcher.d/           Hooks/scripts

NMTUI
──────────────────────────────────────────────
nmtui                   Menú principal
nmtui edit              Editar conexiones
nmtui connect           Activar/desactivar
nmtui hostname          Cambiar hostname
```

---

## 15. Ejercicios

### Ejercicio 1: Explorar tu NetworkManager

Si tu sistema usa NetworkManager:

1. Lista todos los dispositivos con `nmcli device status`. ¿Cuáles están gestionados por NM y cuáles no?
2. Lista todas las conexiones con `nmcli connection show`. ¿Cuántas hay? ¿Cuáles están activas?
3. Muestra los detalles completos de tu conexión activa. Identifica: método (DHCP o manual), IP, gateway, DNS, lease time.
4. Encuentra el archivo de perfil en `/etc/NetworkManager/system-connections/`. ¿Qué formato usa (keyfile o ifcfg)?
5. Verifica qué plugin DNS usa NM: `grep dns /etc/NetworkManager/NetworkManager.conf`

### Ejercicio 2: Crear perfiles alternativos

Crea dos perfiles para la misma interfaz ethernet (usa una VM para no afectar tu red):

1. Perfil "DHCP-Normal": método auto, sin modificaciones.
2. Perfil "Lab-Static": IP 10.10.10.50/24, gateway 10.10.10.1, DNS 10.10.10.1, dominio de búsqueda "lab.internal".

Luego:
3. Alterna entre ambos con `nmcli connection up "X"`. Verifica con `ip addr show` que la IP cambia.
4. Configura "Lab-Static" con autoconnect-priority mayor. ¿Cuál se activa al reiniciar?
5. Elimina el perfil "Lab-Static" y verifica que desapareció del directorio de system-connections.

### Ejercicio 3: Diagnosticar y reparar

Un servidor RHEL 9 tiene este problema: `nmcli device status` muestra eth0 como "connected" pero no hay resolución DNS. La salida de `nmcli device show eth0` muestra:

```
IP4.DNS[1]:   10.0.0.53
```

Sin embargo, `dig google.com` falla con `connection timed out; no servers could be reached`.

1. ¿Dónde buscarías primero? (Pista: ¿qué hay en /etc/resolv.conf?)
2. Si `/etc/resolv.conf` apunta a `127.0.0.53` pero `systemctl is-active systemd-resolved` dice "inactive", ¿cuál es el problema?
3. ¿Cómo configurarías NM para que escriba directamente en /etc/resolv.conf sin depender de resolved?
4. Si el DNS 10.0.0.53 es correcto pero está en otra subred, ¿qué más verificarías?

**Pregunta de reflexión**: En un entorno empresarial con 200 estaciones de trabajo, un administrador quiere cambiar el DNS de todas las máquinas de 10.0.0.53 a 10.0.0.54. Las máquinas usan NetworkManager con DHCP. ¿Es mejor cambiar el DNS en el servidor DHCP o en cada cliente con nmcli? ¿Qué pasaría si la mitad de las máquinas usan perfiles con `ipv4.ignore-auto-dns yes`?

---

> **Siguiente tema**: T03 — systemd-networkd (.network files, cuándo usar sobre NetworkManager)
