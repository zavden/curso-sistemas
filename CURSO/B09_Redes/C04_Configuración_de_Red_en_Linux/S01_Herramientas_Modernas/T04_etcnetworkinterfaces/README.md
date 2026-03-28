# T04 — /etc/network/interfaces: Configuración Clásica Debian

## Índice

1. [Contexto histórico y estado actual](#1-contexto-histórico-y-estado-actual)
2. [Arquitectura de ifupdown](#2-arquitectura-de-ifupdown)
3. [Sintaxis de /etc/network/interfaces](#3-sintaxis-de-etcnetworkinterfaces)
4. [Configurar DHCP](#4-configurar-dhcp)
5. [Configurar IP estática](#5-configurar-ip-estática)
6. [Interfaces virtuales: VLAN, bridge, bond](#6-interfaces-virtuales-vlan-bridge-bond)
7. [Hooks: pre-up, post-up, pre-down, post-down](#7-hooks-pre-up-post-up-pre-down-post-down)
8. [Archivos source y modularización](#8-archivos-source-y-modularización)
9. [ifup, ifdown, ifquery](#9-ifup-ifdown-ifquery)
10. [Interacción con otros gestores](#10-interacción-con-otros-gestores)
11. [Migración a gestores modernos](#11-migración-a-gestores-modernos)
12. [Errores comunes](#12-errores-comunes)
13. [Cheatsheet](#13-cheatsheet)
14. [Ejercicios](#14-ejercicios)

---

## 1. Contexto histórico y estado actual

`/etc/network/interfaces` y su herramienta asociada `ifupdown` fueron durante años **el** método estándar para configurar la red en Debian y derivados (Ubuntu, Raspbian, etc.). Es un sistema simple, basado en un archivo de texto plano y comandos `ifup`/`ifdown`.

```
Timeline de gestores de red en Debian/Ubuntu:

1999─────2004─────2010─────2016─────2020─────2024──►

Debian:
  ifupdown ════════════════════════════════════════►
  (siempre disponible, sigue funcional)
                   NetworkManager ═════════════════►
                   (desktop)
                              systemd-networkd ════►
                              (opcional)

Ubuntu Desktop:
  ifupdown ════════╗
                   NetworkManager ═════════════════►
                              netplan ═════════════►
                              (capa de abstracción)

Ubuntu Server:
  ifupdown ════════════════════╗
                              netplan + networkd ══►
                              (default desde 18.04)
```

**¿Sigue siendo relevante?**

| Contexto | ¿Usar ifupdown? |
|----------|-----------------|
| Debian estable (servidores) | Sí, sigue siendo la opción por defecto si no instalas NM |
| Ubuntu Server moderno | No, netplan es el default |
| Raspberry Pi OS | Sí, aún es el método principal |
| Contenedores Debian | Raramente (el runtime gestiona la red) |
| Sistemas legacy | Sí, encontrarás esta configuración en producción |
| Configuración nueva | Considera systemd-networkd o NetworkManager |

Conocer ifupdown sigue siendo necesario porque:
- Muchos servidores Debian en producción lo usan
- Documentación y scripts legacy lo referencian
- Es el sistema de configuración más simple de entender
- Raspberry Pi y sistemas embebidos basados en Debian lo usan

---

## 2. Arquitectura de ifupdown

ifupdown es extremadamente simple comparado con NetworkManager o systemd-networkd:

```
┌──────────────────────────────────┐
│   /etc/network/interfaces        │  Archivo de configuración
└────────────────┬─────────────────┘
                 │
                 ▼
┌──────────────────────────────────┐
│   ifup / ifdown                  │  Comandos que leen el archivo
│   (no es un daemon, son          │  y configuran la interfaz
│    herramientas que se ejecutan   │
│    y terminan)                   │
└────────────────┬─────────────────┘
                 │
                 │  Ejecutan:
                 ├── ip link set up/down
                 ├── ip addr add/del
                 ├── ip route add/del
                 ├── dhclient (si DHCP)
                 └── scripts hook (pre-up, post-up, etc.)
                 │
                 ▼
┌──────────────────────────────────┐
│   /run/network/ifstate           │  Estado actual
│   (qué interfaces están UP)     │  (archivo simple de texto)
└──────────────────────────────────┘
```

Diferencia fundamental con NetworkManager y systemd-networkd:

```
NetworkManager / systemd-networkd:
  → Son DAEMONS que corren permanentemente
  → Monitorean cambios (cable, DHCP, eventos)
  → Reaccionan automáticamente

ifupdown:
  → Son COMANDOS que se ejecutan y terminan
  → No monitorizan nada
  → Si se desconecta el cable, no pasa nada automáticamente
  → Para renovar DHCP, depende de dhclient (proceso separado)
```

---

## 3. Sintaxis de /etc/network/interfaces

### Estructura básica

```
# /etc/network/interfaces

# Líneas que comienzan con # son comentarios

# Interfaz loopback (siempre necesaria)
auto lo
iface lo inet loopback

# Interfaz ethernet con DHCP
auto eth0
iface eth0 inet dhcp

# Interfaz con IP estática
auto eth1
iface eth1 inet static
    address 192.168.1.50/24
    gateway 192.168.1.1
    dns-nameservers 192.168.1.1 8.8.8.8
    dns-search empresa.local
```

### Directivas clave

```
auto IFACE
  → Activar esta interfaz automáticamente al arrancar
  → Es lo que networking.service busca al inicio

allow-hotplug IFACE
  → Activar cuando el kernel detecta el hardware
  → Más apropiado para interfaces que pueden no existir
  → Usa eventos udev en vez del boot sequence

iface IFACE FAMILY METHOD
  → Define cómo configurar la interfaz
  → FAMILY: inet (IPv4) o inet6 (IPv6)
  → METHOD: dhcp, static, loopback, manual
```

**`auto` vs `allow-hotplug`**:

```
auto eth0:
  → Se activa durante el boot (networking.service)
  → Si la interfaz no existe al arrancar → error/timeout
  → Ideal para interfaces que siempre están presentes

allow-hotplug eth0:
  → Se activa cuando udev detecta la interfaz
  → Si la interfaz no existe al arrancar → sin error
  → Se activa al conectar un adaptador USB, por ejemplo
  → Ideal para interfaces que pueden aparecer y desaparecer

# Pueden combinarse (raro pero válido):
auto eth0
allow-hotplug eth0
```

### Métodos disponibles

| Método | Uso |
|--------|-----|
| `dhcp` | Obtener configuración por DHCP (requiere dhclient o dhcpcd) |
| `static` | Configuración manual completa |
| `loopback` | Solo para la interfaz lo |
| `manual` | Activar la interfaz sin configurar IP (para bridges, bonds) |

---

## 4. Configurar DHCP

```
# /etc/network/interfaces

auto lo
iface lo inet loopback

# DHCP simple
auto eth0
iface eth0 inet dhcp
```

Eso es todo lo necesario. Al hacer `ifup eth0`, ifupdown ejecuta `dhclient eth0` internamente.

### DHCP con opciones adicionales

```
auto eth0
iface eth0 inet dhcp
    # Enviar hostname al servidor DHCP
    hostname mi-servidor

    # Cliente DHCP específico (por defecto usa dhclient)
    # dhcp-client dhclient

    # Pre-configurar el MTU antes de DHCP
    pre-up ip link set eth0 mtu 9000
```

### DHCP para IPv4 + IPv6

```
# IPv4 por DHCP
auto eth0
iface eth0 inet dhcp

# IPv6 por autoconfiguración (SLAAC)
iface eth0 inet6 auto

# O IPv6 por DHCPv6
# iface eth0 inet6 dhcp
```

> **Nota**: una interfaz puede tener múltiples bloques `iface` (uno para inet, otro para inet6). Ambos se aplican.

---

## 5. Configurar IP estática

### Configuración básica

```
auto eth0
iface eth0 inet static
    address 192.168.1.50/24
    gateway 192.168.1.1
    dns-nameservers 192.168.1.1 8.8.8.8
    dns-search empresa.local
```

### Formato legacy (sin CIDR)

En versiones antiguas de ifupdown, la máscara se especificaba por separado:

```
# Formato legacy (aún funciona)
iface eth0 inet static
    address 192.168.1.50
    netmask 255.255.255.0
    network 192.168.1.0
    broadcast 192.168.1.255
    gateway 192.168.1.1

# Formato moderno (preferido)
iface eth0 inet static
    address 192.168.1.50/24
    gateway 192.168.1.1
```

Con el formato CIDR (`/24`), `network` y `broadcast` se calculan automáticamente.

### Múltiples direcciones IP

```
# Método 1: alias de interfaz (legacy, compatible con ifconfig)
auto eth0
iface eth0 inet static
    address 192.168.1.50/24
    gateway 192.168.1.1

auto eth0:1
iface eth0:1 inet static
    address 10.0.0.1/24

auto eth0:2
iface eth0:2 inet static
    address 172.16.0.5/24

# Método 2: post-up con ip addr (moderno)
auto eth0
iface eth0 inet static
    address 192.168.1.50/24
    gateway 192.168.1.1
    post-up ip addr add 10.0.0.1/24 dev eth0
    pre-down ip addr del 10.0.0.1/24 dev eth0
```

### IPv4 + IPv6 estática

```
auto eth0
iface eth0 inet static
    address 192.168.1.50/24
    gateway 192.168.1.1
    dns-nameservers 192.168.1.1

iface eth0 inet6 static
    address 2001:db8::50/64
    gateway 2001:db8::1
    dns-nameservers 2001:4860:4860::8888
```

### Rutas estáticas

```
auto eth0
iface eth0 inet static
    address 192.168.1.50/24
    gateway 192.168.1.1

    # Rutas adicionales
    up ip route add 10.0.0.0/8 via 192.168.1.254
    up ip route add 172.16.0.0/12 via 192.168.1.253
    down ip route del 10.0.0.0/8 via 192.168.1.254
    down ip route del 172.16.0.0/12 via 192.168.1.253
```

### DNS con resolvconf

Las directivas `dns-*` no son parte de ifupdown nativo. Las proporciona el paquete `resolvconf` (o `openresolv`), que actúa como intermediario para `/etc/resolv.conf`:

```
# Estas directivas requieren resolvconf instalado
dns-nameservers 192.168.1.1 8.8.8.8
dns-search empresa.local
dns-domain empresa.local

# Sin resolvconf, debes editar /etc/resolv.conf manualmente
# o usar post-up para escribirlo
```

```bash
# Verificar si resolvconf está instalado
dpkg -l resolvconf 2>/dev/null || dpkg -l openresolv 2>/dev/null
```

---

## 6. Interfaces virtuales: VLAN, bridge, bond

### VLAN (802.1Q)

Requiere el paquete `vlan`:

```bash
sudo apt install vlan
```

```
# /etc/network/interfaces

auto eth0
iface eth0 inet manual

# VLAN 100 sobre eth0
auto eth0.100
iface eth0.100 inet static
    address 10.100.0.5/24
    gateway 10.100.0.1
    vlan-raw-device eth0

# VLAN 200 sobre eth0
auto eth0.200
iface eth0.200 inet static
    address 10.200.0.5/24
```

El nombre `eth0.100` es una convención. ifupdown interpreta el número después del punto como el VLAN ID si el paquete `vlan` está instalado.

### Bridge

Requiere el paquete `bridge-utils`:

```bash
sudo apt install bridge-utils
```

```
# /etc/network/interfaces

# La interfaz física NO tiene IP (es miembro del bridge)
auto eth0
iface eth0 inet manual

# El bridge tiene la IP
auto br0
iface br0 inet static
    address 192.168.1.50/24
    gateway 192.168.1.1
    dns-nameservers 192.168.1.1

    bridge_ports eth0
    bridge_stp on
    bridge_fd 4
    bridge_maxwait 10
```

Parámetros del bridge:

| Parámetro | Significado |
|-----------|-------------|
| `bridge_ports` | Interfaces miembros del bridge |
| `bridge_stp` | Spanning Tree Protocol (on/off) |
| `bridge_fd` | Forward delay (segundos) |
| `bridge_maxwait` | Tiempo máximo de espera para STP |
| `bridge_hello` | Intervalo de hello STP |
| `bridge_maxage` | Máximo age de BPDUs |

Bridge con múltiples interfaces:

```
auto br0
iface br0 inet static
    address 192.168.1.50/24
    gateway 192.168.1.1
    bridge_ports eth0 eth1 eth2
    bridge_stp on
```

Bridge con DHCP (para virtualización):

```
auto br0
iface br0 inet dhcp
    bridge_ports eth0
    bridge_stp off
    bridge_fd 0
```

### Bond (agregación de enlaces)

Requiere el paquete `ifenslave`:

```bash
sudo apt install ifenslave
```

```
# /etc/network/interfaces

# Interfaces miembros (sin IP propia)
auto eth0
iface eth0 inet manual
    bond-master bond0

auto eth1
iface eth1 inet manual
    bond-master bond0

# Interfaz bond
auto bond0
iface bond0 inet static
    address 192.168.1.50/24
    gateway 192.168.1.1

    bond-slaves eth0 eth1
    bond-mode 802.3ad
    bond-miimon 100
    bond-lacp-rate fast
    bond-xmit-hash-policy layer3+4
```

Modos de bonding:

| Modo | Nombre | Uso |
|------|--------|-----|
| 0 | balance-rr | Round-robin, requiere switch config |
| 1 | active-backup | Un enlace activo, otro en standby |
| 2 | balance-xor | Hash XOR |
| 3 | broadcast | Envía por todos los enlaces |
| 4 | 802.3ad | LACP (requiere switch con LACP) |
| 5 | balance-tlb | Balanceo adaptativo de transmisión |
| 6 | balance-alb | Balanceo adaptativo (tx + rx) |

---

## 7. Hooks: pre-up, post-up, pre-down, post-down

Los hooks permiten ejecutar comandos en momentos específicos del ciclo de vida de una interfaz:

```
Orden de ejecución:

ifup eth0:
  1. pre-up        ← Antes de activar la interfaz
  2. (interfaz se activa, IP se asigna)
  3. up / post-up  ← Después de activar (up es alias de post-up)

ifdown eth0:
  4. down / pre-down ← Antes de desactivar (down es alias de pre-down)
  5. (interfaz se desactiva, IP se elimina)
  6. post-down     ← Después de desactivar
```

```
auto eth0
iface eth0 inet static
    address 192.168.1.50/24
    gateway 192.168.1.1

    # Antes de activar: cargar módulo si hace falta
    pre-up modprobe 8021q || true

    # Después de activar: agregar ruta, registrar en log
    post-up ip route add 10.0.0.0/8 via 192.168.1.254
    post-up logger "eth0 is up with IP 192.168.1.50"

    # Antes de desactivar: limpiar rutas
    pre-down ip route del 10.0.0.0/8 via 192.168.1.254

    # Después de desactivar: registrar
    post-down logger "eth0 is down"
```

### Hooks con scripts externos

```
auto eth0
iface eth0 inet dhcp
    post-up /usr/local/bin/on-network-up.sh
    pre-down /usr/local/bin/on-network-down.sh
```

### El `|| true` en hooks

Si un hook falla (exit code ≠ 0), `ifup` aborta toda la operación. Para evitar que un hook opcional detenga la configuración:

```
# SIN || true: si el comando falla, ifup aborta
pre-up modprobe bonding

# CON || true: si falla, ifup continúa de todos modos
pre-up modprobe bonding || true

# Usar || true cuando el hook es "nice to have" pero no crítico
```

### Directorio de scripts por fase

Además de hooks en línea, ifupdown busca scripts en directorios estándar:

```bash
/etc/network/if-pre-up.d/
/etc/network/if-up.d/
/etc/network/if-down.d/
/etc/network/if-post-down.d/
```

Los scripts en estos directorios se ejecutan para **todas** las interfaces. Reciben información vía variables de entorno:

```bash
#!/bin/bash
# /etc/network/if-up.d/log-interface
# Recibe: $IFACE, $METHOD, $MODE, $ADDRFAM, $LOGICAL

logger "Interface $IFACE ($METHOD) is up"
```

```bash
# Los scripts deben ser ejecutables
chmod 755 /etc/network/if-up.d/log-interface
```

---

## 8. Archivos source y modularización

Para archivos grandes, ifupdown permite dividir la configuración en múltiples archivos:

```
# /etc/network/interfaces

# Loopback (siempre aquí)
auto lo
iface lo inet loopback

# Incluir archivo específico
source /etc/network/interfaces.d/eth0

# Incluir todos los archivos de un directorio
source /etc/network/interfaces.d/*

# Con patrón
source /etc/network/interfaces.d/*.cfg
```

```
# /etc/network/interfaces.d/eth0
auto eth0
iface eth0 inet static
    address 192.168.1.50/24
    gateway 192.168.1.1
```

```
# /etc/network/interfaces.d/vlans
auto eth0.100
iface eth0.100 inet static
    address 10.100.0.5/24

auto eth0.200
iface eth0.200 inet static
    address 10.200.0.5/24
```

**Regla de Debian**: el directorio `/etc/network/interfaces.d/` existe por defecto y la directiva `source /etc/network/interfaces.d/*` suele estar incluida en el archivo principal. Puedes crear archivos ahí sin modificar el archivo principal.

> **Cuidado**: no pongas archivos con extensiones como `.bak`, `.orig` o `.dpkg-old` en `interfaces.d/` si usas `source *`, porque ifupdown los intentará parsear y puede fallar.

---

## 9. ifup, ifdown, ifquery

### ifup — Activar interfaz

```bash
# Activar una interfaz según /etc/network/interfaces
sudo ifup eth0

# Activar todas las interfaces marcadas con "auto"
sudo ifup -a

# Activar con verbose (muestra qué hace)
sudo ifup -v eth0

# No ejecutar realmente, solo mostrar qué haría (dry run)
sudo ifup -n eth0

# Forzar activación aunque ifstate diga que ya está UP
sudo ifup --force eth0
```

### ifdown — Desactivar interfaz

```bash
# Desactivar interfaz
sudo ifdown eth0

# Desactivar todas
sudo ifdown -a

# Forzar desactivación
sudo ifdown --force eth0
```

### ifquery — Consultar configuración

```bash
# Mostrar configuración definida para una interfaz
ifquery eth0
```

```
auto eth0
iface eth0 inet static
    address 192.168.1.50/24
    gateway 192.168.1.1
    dns-nameservers 192.168.1.1 8.8.8.8
```

```bash
# Listar todas las interfaces configuradas con auto
ifquery --list
```

```
lo
eth0
eth0.100
```

```bash
# Verificar sintaxis del archivo (sin aplicar)
ifquery --syntax-check
```

### El archivo ifstate

`ifup`/`ifdown` mantienen un registro de qué interfaces están activas:

```bash
cat /run/network/ifstate
```

```
lo=lo
eth0=eth0
```

Si este archivo se desincroniza con la realidad (por ejemplo, tras usar `ip link set down` directamente), `ifdown` puede negarse a operar. La solución:

```bash
# Forzar ifdown aunque ifstate no coincida
sudo ifdown --force eth0

# O editar ifstate manualmente (último recurso)
```

### Reiniciar toda la red

```bash
# Vía systemd (método preferido)
sudo systemctl restart networking

# O manualmente
sudo ifdown -a && sudo ifup -a
```

> **Precaución SSH**: si estás conectado por SSH y reinicias networking, **perderás la conexión** si la IP cambia. Usa `nohup` o ejecuta ambos comandos en la misma línea.

---

## 10. Interacción con otros gestores

### ifupdown + NetworkManager

Por defecto, NetworkManager **no gestiona** las interfaces definidas en `/etc/network/interfaces`. Este es un comportamiento configurable:

```ini
# /etc/NetworkManager/NetworkManager.conf
[main]
plugins=ifupdown,keyfile

[ifupdown]
managed=false     # false = NM NO toca interfaces de /etc/network/interfaces
                  # true  = NM gestiona también esas interfaces
```

```
Con managed=false (default):

/etc/network/interfaces define eth0
  → ifupdown gestiona eth0
  → NM muestra eth0 como "unmanaged"
  → NM gestiona solo wlan0 y otras no definidas en interfaces
```

### ifupdown + systemd-networkd

No se recomienda usar ambos para las mismas interfaces. Si necesitas systemd-networkd, desactiva ifupdown:

```bash
# Desactivar networking.service (ifupdown)
sudo systemctl disable networking

# Activar systemd-networkd
sudo systemctl enable --now systemd-networkd
```

### networking.service

ifupdown se integra con systemd a través del servicio `networking`:

```bash
# Estado
systemctl status networking

# Reiniciar (ejecuta ifdown -a && ifup -a)
sudo systemctl restart networking

# Este servicio es de tipo "oneshot": ejecuta ifup al start
# y ifdown al stop, no es un daemon permanente
```

---

## 11. Migración a gestores modernos

### De ifupdown a systemd-networkd

```
/etc/network/interfaces:          →    /etc/systemd/network/25-eth0.network:

auto eth0                               [Match]
iface eth0 inet static                  Name=eth0
    address 192.168.1.50/24
    gateway 192.168.1.1                  [Network]
    dns-nameservers 1.1.1.1 8.8.8.8     Address=192.168.1.50/24
    dns-search empresa.local             Gateway=192.168.1.1
                                         DNS=1.1.1.1
                                         DNS=8.8.8.8
                                         Domains=empresa.local
```

```
DHCP:

auto eth0                               [Match]
iface eth0 inet dhcp                     Name=eth0

                                         [Network]
                                         DHCP=yes
```

```
Bridge:

auto br0                                [NetDev]     (20-br0.netdev)
iface br0 inet static                   Name=br0
    address 192.168.1.50/24             Kind=bridge
    bridge_ports eth0
    bridge_stp on                        [Match]      (25-br0.network)
                                         Name=br0
auto eth0                               [Network]
iface eth0 inet manual                   Address=192.168.1.50/24

                                         [Match]      (30-eth0.network)
                                         Name=eth0
                                         [Network]
                                         Bridge=br0
```

### De ifupdown a NetworkManager

```bash
# 1. Verificar que NM está instalado
apt install network-manager

# 2. Comentar o vaciar las interfaces en /etc/network/interfaces
#    (dejar solo lo)

# 3. Configurar NM para gestionar las interfaces
# /etc/NetworkManager/NetworkManager.conf
# [ifupdown]
# managed=true

# 4. Crear perfiles con nmcli
nmcli connection add type ethernet con-name "eth0" ifname eth0 \
    ipv4.method manual \
    ipv4.addresses "192.168.1.50/24" \
    ipv4.gateway "192.168.1.1" \
    ipv4.dns "1.1.1.1,8.8.8.8"

# 5. Desactivar ifupdown, activar NM
sudo systemctl disable networking
sudo systemctl enable --now NetworkManager
```

### Pasos seguros de migración (vía SSH)

```bash
# 1. Preparar la nueva configuración SIN activarla
# 2. Crear un script de rollback con timer:
cat > /tmp/rollback.sh << 'SCRIPT'
#!/bin/bash
systemctl stop systemd-networkd
systemctl start networking
SCRIPT
chmod +x /tmp/rollback.sh

# 3. Programar rollback automático en 5 minutos
echo "/tmp/rollback.sh" | at now + 5 minutes

# 4. Hacer el cambio
sudo systemctl stop networking
sudo systemctl start systemd-networkd

# 5. Si funciona y puedes conectar:
atrm $(atq | awk '{print $1}')  # Cancelar rollback

# 6. Si no funciona: esperar 5 minutos, el rollback restaura
```

---

## 12. Errores comunes

### Error 1: Olvidar `auto` o `allow-hotplug`

```
INCORRECTO:
# /etc/network/interfaces
iface eth0 inet dhcp
# "La interfaz no sube al arrancar"

# Sin "auto" ni "allow-hotplug", ifup -a (que ejecuta
# networking.service al boot) no activa esta interfaz

CORRECTO:
auto eth0
iface eth0 inet dhcp

# O:
allow-hotplug eth0
iface eth0 inet dhcp
```

### Error 2: Editar /etc/network/interfaces sin reiniciar la interfaz

```
INCORRECTO:
sudo vim /etc/network/interfaces
# Cambié la IP de eth0
# "¿Por qué sigue con la IP anterior?"

# ifupdown no monitoriza el archivo. Los cambios no se aplican solos.

CORRECTO:
sudo vim /etc/network/interfaces
sudo ifdown eth0 && sudo ifup eth0

# O reiniciar el servicio:
sudo systemctl restart networking

# PRECAUCIÓN: si estás en SSH, usa una sola línea:
sudo ifdown eth0 && sudo ifup eth0
# Si los separas, ifdown te desconecta y ifup nunca se ejecuta
```

### Error 3: Conflicto ifstate cuando se usó `ip` directamente

```
PROBLEMA:
# Usaste ip directamente para bajar la interfaz
sudo ip link set eth0 down
# Ahora ifdown eth0 dice "interface eth0 not configured"
# Porque ifstate aún la marca como UP

SOLUCIÓN:
# Forzar ifdown
sudo ifdown --force eth0

# Y luego subir normalmente
sudo ifup eth0
```

### Error 4: dns-nameservers no funciona sin resolvconf

```
PROBLEMA:
iface eth0 inet static
    address 192.168.1.50/24
    gateway 192.168.1.1
    dns-nameservers 8.8.8.8
# /etc/resolv.conf no cambia

CAUSA:
# dns-nameservers requiere el paquete resolvconf instalado
dpkg -l resolvconf
# → no instalado

SOLUCIÓN:
# Opción 1: instalar resolvconf
sudo apt install resolvconf

# Opción 2: escribir resolv.conf manualmente con hook
iface eth0 inet static
    address 192.168.1.50/24
    gateway 192.168.1.1
    post-up echo "nameserver 8.8.8.8" > /etc/resolv.conf
    post-up echo "nameserver 1.1.1.1" >> /etc/resolv.conf

# Opción 3: editar /etc/resolv.conf directamente (si nada lo sobreescribe)
```

### Error 5: Dejar archivos .bak en interfaces.d/ con source *

```
PROBLEMA:
# /etc/network/interfaces tiene:
source /etc/network/interfaces.d/*

# En el directorio hay:
/etc/network/interfaces.d/eth0
/etc/network/interfaces.d/eth0.bak      ← backup accidental
/etc/network/interfaces.d/eth0.dpkg-old ← de apt upgrade

# ifupdown intenta parsear TODOS los archivos → errores de sintaxis
# o configuración duplicada

CORRECTO:
# Eliminar archivos que no son configuración válida
rm /etc/network/interfaces.d/eth0.bak
rm /etc/network/interfaces.d/eth0.dpkg-old

# O usar un patrón más restrictivo en source
source /etc/network/interfaces.d/*.cfg

# Y nombrar los archivos con extensión .cfg
```

---

## 13. Cheatsheet

```
ARCHIVO PRINCIPAL
──────────────────────────────────────────────
/etc/network/interfaces                Configuración
/etc/network/interfaces.d/            Drop-ins
/run/network/ifstate                   Estado actual

DIRECTIVAS
──────────────────────────────────────────────
auto IFACE                Activar al boot
allow-hotplug IFACE       Activar al detectar hardware
iface IFACE inet METHOD   Configurar IPv4
iface IFACE inet6 METHOD  Configurar IPv6
source FILE               Incluir otro archivo

MÉTODOS
──────────────────────────────────────────────
dhcp       DHCP (requiere dhclient)
static     IP manual (address, gateway, etc.)
manual     Sin IP (para bridges, bonds)
loopback   Solo para lo

OPCIONES ESTÁTICAS
──────────────────────────────────────────────
address 192.168.1.50/24      IP con máscara CIDR
gateway 192.168.1.1          Ruta por defecto
dns-nameservers 8.8.8.8      DNS (requiere resolvconf)
dns-search empresa.local     Dominio de búsqueda
mtu 9000                     MTU

HOOKS (en orden de ejecución)
──────────────────────────────────────────────
pre-up CMD       Antes de activar
post-up CMD      Después de activar (alias: up)
pre-down CMD     Antes de desactivar (alias: down)
post-down CMD    Después de desactivar

Directorios globales:
  /etc/network/if-pre-up.d/
  /etc/network/if-up.d/
  /etc/network/if-down.d/
  /etc/network/if-post-down.d/

COMANDOS
──────────────────────────────────────────────
ifup eth0              Activar interfaz
ifup -a                Activar todas (auto)
ifdown eth0            Desactivar interfaz
ifdown -a              Desactivar todas
ifup -v eth0           Verbose
ifup -n eth0           Dry run
ifdown --force eth0    Forzar (ignorar ifstate)
ifquery eth0           Ver configuración
ifquery --list         Listar interfaces auto
ifquery --syntax-check Verificar sintaxis

systemctl restart networking    Reiniciar todo

INTERFACES VIRTUALES
──────────────────────────────────────────────
VLAN:    iface eth0.100 inet static
Bridge:  bridge_ports eth0
         bridge_stp on
Bond:    bond-slaves eth0 eth1
         bond-mode 802.3ad

EQUIVALENCIAS
──────────────────────────────────────────────
ifupdown              NM                    systemd-networkd
──────────────────────────────────────────────
/etc/network/         nmcli con             /etc/systemd/network/
interfaces            modify                *.network

ifup eth0             nmcli con up          networkctl up eth0
ifdown eth0           nmcli device          networkctl down eth0
                      disconnect
ifup -a               systemctl restart     systemctl restart
                      NetworkManager        systemd-networkd
```

---

## 14. Ejercicios

### Ejercicio 1: Leer y entender una configuración existente

Dado este archivo `/etc/network/interfaces`:

```
auto lo
iface lo inet loopback

auto eth0
iface eth0 inet manual

auto br0
iface br0 inet static
    address 192.168.1.10/24
    gateway 192.168.1.1
    dns-nameservers 8.8.8.8 1.1.1.1
    bridge_ports eth0
    bridge_stp on
    bridge_fd 4
    post-up iptables -A FORWARD -i br0 -o br0 -j ACCEPT
    pre-down iptables -D FORWARD -i br0 -o br0 -j ACCEPT

allow-hotplug eth1
iface eth1 inet dhcp

auto eth0.100
iface eth0.100 inet static
    address 10.100.0.5/24
    vlan-raw-device eth0
```

Responde:
1. ¿Cuántas interfaces se activan al arrancar con `auto`? ¿Cuáles?
2. ¿Por qué eth0 usa el método `manual` y no tiene IP?
3. ¿Qué hace la regla iptables del post-up de br0?
4. ¿Por qué eth1 usa `allow-hotplug` en vez de `auto`?
5. ¿Hay un problema potencial con la VLAN 100? (Pista: eth0 es miembro de br0)
6. Si se desconecta el cable de eth1 y se vuelve a conectar, ¿qué pasa?

### Ejercicio 2: Escribir configuración desde cero

Un servidor Debian tiene tres interfaces: eth0, eth1, eth2. Escribe `/etc/network/interfaces` para:

- **eth0**: DHCP, se activa al boot
- **eth1**: IP estática 10.0.0.1/24, sin gateway (red interna), DNS 10.0.0.53
- **eth2**: Parte de un bond con eth3 (que no existe todavía), modo active-backup, IP del bond 172.16.0.10/24, gateway 172.16.0.1
- Agrega un hook post-up en eth0 que registre la IP obtenida en syslog
- Agrega una ruta estática en eth1: la red 10.10.0.0/16 va por 10.0.0.254

### Ejercicio 3: Migración planificada

Tienes este servidor Debian con ifupdown y debes migrarlo a systemd-networkd. La configuración actual:

```
auto lo
iface lo inet loopback

auto eth0
iface eth0 inet static
    address 10.0.1.50/24
    gateway 10.0.1.1
    dns-nameservers 10.0.1.1 8.8.8.8
    dns-search prod.empresa.local
    post-up ip route add 172.16.0.0/12 via 10.0.1.254
    pre-down ip route del 172.16.0.0/12 via 10.0.1.254
    mtu 9000
```

1. Escribe los archivos `.network` y `.link` equivalentes para systemd-networkd.
2. ¿Cómo manejas la ruta estática que está en un hook post-up?
3. Describe el procedimiento de migración paso a paso, asumiendo que estás conectado por SSH a 10.0.1.50. ¿Cómo te proteges contra perder la conexión?
4. ¿Cómo verificas que la migración fue exitosa?

**Pregunta de reflexión**: Un administrador junior argumenta que ifupdown es "obsoleto y debería eliminarse de todos los servidores." Un administrador senior responde que "si funciona, no lo toques." ¿Con quién estás más de acuerdo? ¿Qué factores evaluarías para decidir si migrar un servidor en producción que lleva 5 años funcionando con ifupdown sin problemas?

---

> **Siguiente sección**: S02 — Configuración Avanzada (Hostname, Bonding y teaming, VLANs, Bridge networking, VPN tunnels)
