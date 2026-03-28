# T05 — VPN Tunnels: WireGuard y Comparación con OpenVPN

## Índice

1. [¿Qué es un túnel VPN?](#1-qué-es-un-túnel-vpn)
2. [Tipos de VPN](#2-tipos-de-vpn)
3. [WireGuard: arquitectura](#3-wireguard-arquitectura)
4. [WireGuard: generar claves](#4-wireguard-generar-claves)
5. [WireGuard: configuración manual](#5-wireguard-configuración-manual)
6. [WireGuard: wg-quick](#6-wireguard-wg-quick)
7. [WireGuard: escenarios comunes](#7-wireguard-escenarios-comunes)
8. [WireGuard con NetworkManager y systemd-networkd](#8-wireguard-con-networkmanager-y-systemd-networkd)
9. [OpenVPN: visión general](#9-openvpn-visión-general)
10. [WireGuard vs OpenVPN](#10-wireguard-vs-openvpn)
11. [Diagnóstico de túneles VPN](#11-diagnóstico-de-túneles-vpn)
12. [Errores comunes](#12-errores-comunes)
13. [Cheatsheet](#13-cheatsheet)
14. [Ejercicios](#14-ejercicios)

---

## 1. ¿Qué es un túnel VPN?

Un túnel VPN (Virtual Private Network) crea una conexión cifrada entre dos puntos a través de una red no confiable (típicamente Internet). Los paquetes que viajan por el túnel están encapsulados y cifrados, invisibles para cualquier observador intermedio.

```
Sin VPN:

Oficina A                     Internet                    Oficina B
[192.168.1.0/24]  ──────── (tráfico visible) ────────  [192.168.2.0/24]
                            Cualquiera puede
                            interceptar los datos

Con VPN:

Oficina A                     Internet                    Oficina B
[192.168.1.0/24]  ═══════ (túnel cifrado) ═══════════  [192.168.2.0/24]
       │                                                      │
       └── Los hosts de A pueden acceder                      │
           a los hosts de B como si                           │
           estuvieran en la misma red ────────────────────────┘
```

### Encapsulación

El túnel funciona envolviendo los paquetes originales dentro de paquetes nuevos:

```
Paquete original:
┌──────────┬──────────┬──────────────┐
│ IP Header│ TCP/UDP  │   Payload    │
│ src→dst  │ Header   │   (datos)    │
└──────────┴──────────┴──────────────┘

Paquete encapsulado (dentro del túnel):
┌──────────┬──────────┬─────────┬──────────────────────────────┐
│ IP Outer │ UDP      │ VPN     │ ┌──────────┬───────┬───────┐ │
│ A_pub →  │ Header   │ Header  │ │ IP Header│TCP/UDP│Payload│ │
│ B_pub    │ port 51820│(cifrado)│ │ src→dst  │Header │(datos)│ │
└──────────┴──────────┴─────────┴─└──────────┴───────┴───────┘─┘
                                    └── Paquete original cifrado ──┘
│◄──── Cabecera de transporte ────►│◄──── Contenido cifrado ──────►│

Un observador en Internet solo ve:
  - IP pública A → IP pública B
  - Puerto UDP 51820
  - Datos cifrados (ilegibles)
```

---

## 2. Tipos de VPN

```
VPN Site-to-Site:
  Conecta dos redes completas
  Router/firewall en cada extremo mantiene el túnel
  Los hosts no saben que hay VPN (transparente)

  Oficina A ════════════ Oficina B
  [red entera]           [red entera]

VPN Client-to-Site (Remote Access):
  Un dispositivo se conecta a la red de la empresa
  El laptop del empleado remoto accede a recursos internos

  Laptop ════════════ Oficina
  [un host]           [red entera]

VPN Client-to-Client (Mesh / Peer-to-Peer):
  Múltiples hosts se conectan entre sí directamente
  Cada host puede alcanzar a los demás

  Host A ══════ Host B
    ║              ║
    ╚══════ Host C ╝
```

### Capa de operación

| VPN | Capa | Transporta | Ejemplo |
|-----|------|------------|---------|
| L2 (TAP) | Enlace | Tramas Ethernet completas | OpenVPN TAP, L2TP |
| L3 (TUN) | Red | Solo paquetes IP | WireGuard, OpenVPN TUN, IPsec |

```
L2 VPN (TAP):
  - Las máquinas remotas parecen estar en el mismo segmento Ethernet
  - Broadcast, ARP, DHCP cruzan el túnel
  - Más overhead pero permite bridging

L3 VPN (TUN):
  - Solo tráfico IP cruza el túnel
  - Más eficiente (sin broadcast Ethernet)
  - No permite bridging
  - WireGuard SOLO opera en L3
```

---

## 3. WireGuard: arquitectura

WireGuard es una VPN moderna integrada en el kernel Linux (desde 5.6). Su filosofía es radicalmente simple comparada con IPsec u OpenVPN:

```
Comparación de líneas de código:

WireGuard:   ~4,000 líneas (kernel module)
OpenVPN:     ~100,000 líneas (userspace)
IPsec:       ~400,000 líneas (kernel, varios componentes)

Menos código → menos superficie de ataque → más auditable
```

### Modelo conceptual

```
WireGuard opera como una interfaz de red:

┌──────────────────────────────────────────────┐
│                   Host A                      │
│                                              │
│  Aplicación (curl, ssh, ...)                 │
│       │                                      │
│       ▼                                      │
│  wg0 (interfaz WireGuard)                    │
│  IP: 10.0.0.1/24                             │
│       │                                      │
│       │ Cifra el paquete                     │
│       │ Encapsula en UDP                     │
│       ▼                                      │
│  eth0 (interfaz real)                        │
│  IP pública: 203.0.113.1                     │
│       │                                      │
└───────┼──────────────────────────────────────┘
        │  UDP puerto 51820
        │
    ════╪════ Internet (cifrado) ════
        │
┌───────┼──────────────────────────────────────┐
│       │                                      │
│  eth0 (interfaz real)                        │
│  IP pública: 198.51.100.1                    │
│       │                                      │
│       │ Desencapsula UDP                     │
│       │ Descifra el paquete                  │
│       ▼                                      │
│  wg0 (interfaz WireGuard)                    │
│  IP: 10.0.0.2/24                             │
│       │                                      │
│       ▼                                      │
│  Aplicación                                  │
│                                              │
│                   Host B                      │
└──────────────────────────────────────────────┘
```

### Criptografía

WireGuard usa un conjunto fijo de primitivas criptográficas modernas — no hay negociación de cipher suites:

| Función | Algoritmo |
|---------|-----------|
| Cifrado simétrico | ChaCha20 |
| Autenticación | Poly1305 |
| Intercambio de claves | Curve25519 (ECDH) |
| Hash | BLAKE2s |
| Derivación de claves | HKDF |

No hay opciones de configuración criptográfica. Si se descubre una vulnerabilidad en algún algoritmo, se actualiza WireGuard completo (versionamiento criptográfico).

### Modelo de peers

WireGuard no tiene concepto de "cliente" y "servidor". Solo hay **peers** (pares). Cada peer tiene:

- Una **clave privada** (se queda en la máquina)
- Una **clave pública** (se comparte con los otros peers)
- **AllowedIPs**: qué rangos de IP se enrutan por ese peer
- **Endpoint** (opcional): IP:puerto donde contactar al peer

```
Peer A:                              Peer B:
  Clave privada: (secreta)             Clave privada: (secreta)
  Clave pública: PubA                  Clave pública: PubB
  Conoce: PubB                         Conoce: PubA
  AllowedIPs para B: 10.0.0.2/32      AllowedIPs para A: 10.0.0.1/32
  Endpoint de B: 198.51.100.1:51820    Endpoint de A: 203.0.113.1:51820
```

---

## 4. WireGuard: generar claves

```bash
# Instalar WireGuard
# Fedora/RHEL:
sudo dnf install wireguard-tools

# Ubuntu/Debian:
sudo apt install wireguard

# Arch:
sudo pacman -S wireguard-tools
```

### Par de claves

```bash
# Generar clave privada
wg genkey
# Salida: oK56DE9Ue9zK76rAc8pBl6opph+1v36lm7cXXsQKrQM=

# Generar clave pública a partir de la privada
echo "oK56DE9Ue9zK76rAc8pBl6opph+1v36lm7cXXsQKrQM=" | wg pubkey
# Salida: YDcxq2lLth3IP+L45BKGOD4yd5RJHSqGfOEzMGCRSzY=

# Generar ambas en un solo comando
wg genkey | tee privatekey | wg pubkey > publickey

# Ver las claves generadas
cat privatekey
cat publickey
```

### Clave pre-compartida (PSK, opcional)

Añade una capa extra de seguridad (post-quantum resistance):

```bash
wg genpsk
# Salida: GnMT0RYi8XBQZ1mNK1sB5MjDY9mFOVPkPAqVUlY4tso=
```

### Permisos de los archivos de claves

```bash
# Las claves privadas deben ser legibles solo por root
chmod 600 privatekey
chmod 600 /etc/wireguard/wg0.conf

# La clave pública puede ser pública (es el propósito)
```

---

## 5. WireGuard: configuración manual

Configuración paso a paso sin wg-quick, para entender qué pasa internamente:

### Peer A (10.0.0.1)

```bash
# 1. Crear interfaz WireGuard
sudo ip link add wg0 type wireguard

# 2. Asignar IP a la interfaz del túnel
sudo ip addr add 10.0.0.1/24 dev wg0

# 3. Configurar clave privada
sudo wg set wg0 \
    private-key /etc/wireguard/privatekey-a \
    listen-port 51820

# 4. Agregar peer B
sudo wg set wg0 peer <PUBKEY_B> \
    allowed-ips 10.0.0.2/32 \
    endpoint 198.51.100.1:51820

# 5. Subir la interfaz
sudo ip link set wg0 up

# 6. (Opcional) Agregar rutas para redes remotas
sudo ip route add 192.168.2.0/24 dev wg0
```

### Peer B (10.0.0.2)

```bash
sudo ip link add wg0 type wireguard
sudo ip addr add 10.0.0.2/24 dev wg0

sudo wg set wg0 \
    private-key /etc/wireguard/privatekey-b \
    listen-port 51820

sudo wg set wg0 peer <PUBKEY_A> \
    allowed-ips 10.0.0.1/32 \
    endpoint 203.0.113.1:51820

sudo ip link set wg0 up
```

### Ver estado

```bash
sudo wg show
```

```
interface: wg0
  public key: YDcxq2lLth3IP+L45BKGOD4yd5RJHSqGfOEzMGCRSzY=
  private key: (hidden)
  listening port: 51820

peer: xTIBA5rboUvnH4htodjb6e697QjLERt1NAB4mZqp8Dg=
  endpoint: 198.51.100.1:51820
  allowed ips: 10.0.0.2/32
  latest handshake: 42 seconds ago
  transfer: 1.24 MiB received, 2.31 MiB sent
```

---

## 6. WireGuard: wg-quick

`wg-quick` es un wrapper que simplifica la configuración. Lee un archivo de configuración y ejecuta todos los comandos `ip` y `wg` necesarios.

### Archivo de configuración

```ini
# /etc/wireguard/wg0.conf (Peer A — "servidor")

[Interface]
# Clave privada de este peer
PrivateKey = oK56DE9Ue9zK76rAc8pBl6opph+1v36lm7cXXsQKrQM=

# IP del túnel
Address = 10.0.0.1/24

# Puerto de escucha
ListenPort = 51820

# (Opcional) DNS para el túnel
# DNS = 1.1.1.1, 8.8.8.8

# (Opcional) Comandos post-up/post-down
PostUp = iptables -A FORWARD -i wg0 -j ACCEPT; iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
PostDown = iptables -D FORWARD -i wg0 -j ACCEPT; iptables -t nat -D POSTROUTING -o eth0 -j MASQUERADE

# Peer B
[Peer]
PublicKey = xTIBA5rboUvnH4htodjb6e697QjLERt1NAB4mZqp8Dg=
# PSK (opcional, para seguridad extra)
PresharedKey = GnMT0RYi8XBQZ1mNK1sB5MjDY9mFOVPkPAqVUlY4tso=
# Qué IPs se enrutan por este peer
AllowedIPs = 10.0.0.2/32
# Endpoint es opcional en el "servidor" (el cliente conecta a nosotros)
```

```ini
# /etc/wireguard/wg0.conf (Peer B — "cliente")

[Interface]
PrivateKey = SHC5GIh1TqD6Uu5p20MGrNal56mQRpDx0KXVk7SRGWA=
Address = 10.0.0.2/24
DNS = 1.1.1.1

[Peer]
PublicKey = YDcxq2lLth3IP+L45BKGOD4yd5RJHSqGfOEzMGCRSzY=
PresharedKey = GnMT0RYi8XBQZ1mNK1sB5MjDY9mFOVPkPAqVUlY4tso=
AllowedIPs = 10.0.0.0/24, 192.168.1.0/24
Endpoint = 203.0.113.1:51820
# Keepalive: necesario si el cliente está detrás de NAT
PersistentKeepalive = 25
```

### Controlar el túnel

```bash
# Levantar el túnel
sudo wg-quick up wg0

# Bajar el túnel
sudo wg-quick down wg0

# Habilitar al arranque (systemd)
sudo systemctl enable wg-quick@wg0

# Estado
sudo wg show wg0
```

### Qué hace wg-quick internamente

```bash
# wg-quick up wg0 ejecuta (aproximadamente):

ip link add wg0 type wireguard
wg setconf wg0 /dev/fd/63        # Configura claves y peers
ip -4 address add 10.0.0.1/24 dev wg0
ip link set mtu 1420 up dev wg0  # MTU = 1500 - 80 (overhead WG)
ip -4 route add 10.0.0.2/32 dev wg0
# Si hay PostUp, lo ejecuta
# Si hay DNS, configura resolvconf
```

### AllowedIPs como tabla de rutas

`AllowedIPs` cumple una doble función:

```
AllowedIPs funciona como:

1. FILTRO DE ENTRADA:
   Solo acepta paquetes de este peer si el IP origen
   está en AllowedIPs

2. TABLA DE RUTAS:
   wg-quick instala rutas: paquetes con destino en
   AllowedIPs se envían por este peer

Ejemplo:
  AllowedIPs = 10.0.0.2/32, 192.168.2.0/24

  → Acepta paquetes de este peer si vienen de 10.0.0.2
    o de 192.168.2.0/24
  → Envía por este peer paquetes con destino 10.0.0.2
    o 192.168.2.0/24
```

### Full tunnel (todo el tráfico por la VPN)

```ini
# En el cliente: enviar TODO el tráfico por la VPN
[Peer]
PublicKey = ...
AllowedIPs = 0.0.0.0/0, ::/0
Endpoint = 203.0.113.1:51820

# 0.0.0.0/0 = todo IPv4
# ::/0 = todo IPv6
# wg-quick añade reglas de policy routing para evitar
# que el tráfico al endpoint del servidor también
# vaya por el túnel (loop)
```

### Split tunnel (solo cierto tráfico por la VPN)

```ini
# En el cliente: solo tráfico a redes internas por la VPN
[Peer]
PublicKey = ...
AllowedIPs = 10.0.0.0/24, 192.168.1.0/24
Endpoint = 203.0.113.1:51820

# El tráfico a Internet sale directamente (no por la VPN)
# Solo el tráfico a 10.0.0.0/24 y 192.168.1.0/24 usa el túnel
```

---

## 7. WireGuard: escenarios comunes

### Escenario 1: Acceso remoto (road warrior)

Un empleado se conecta desde casa a la red de la oficina:

```
Laptop (casa)                        Servidor (oficina)
10.0.0.2  ══════ Internet ══════  10.0.0.1
                                    │
                                    └── 192.168.1.0/24 (red interna)
                                        ├── 192.168.1.10 (DB)
                                        └── 192.168.1.20 (Intranet)
```

```ini
# Servidor (oficina): /etc/wireguard/wg0.conf
[Interface]
PrivateKey = <server_private_key>
Address = 10.0.0.1/24
ListenPort = 51820
PostUp = iptables -A FORWARD -i wg0 -j ACCEPT; iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
PostDown = iptables -D FORWARD -i wg0 -j ACCEPT; iptables -t nat -D POSTROUTING -o eth0 -j MASQUERADE

[Peer]
# Laptop del empleado
PublicKey = <laptop_public_key>
AllowedIPs = 10.0.0.2/32
```

```ini
# Laptop (casa): /etc/wireguard/wg0.conf
[Interface]
PrivateKey = <laptop_private_key>
Address = 10.0.0.2/24
DNS = 192.168.1.1

[Peer]
PublicKey = <server_public_key>
AllowedIPs = 10.0.0.0/24, 192.168.1.0/24
Endpoint = oficina.empresa.com:51820
PersistentKeepalive = 25
```

```bash
# En el servidor: habilitar IP forwarding
echo "net.ipv4.ip_forward = 1" | sudo tee /etc/sysctl.d/99-wireguard.conf
sudo sysctl -p /etc/sysctl.d/99-wireguard.conf
```

### Escenario 2: Site-to-Site

Conectar dos oficinas:

```
Oficina A (Madrid)                    Oficina B (Barcelona)
192.168.1.0/24                        192.168.2.0/24
       │                                    │
  ┌────┴────┐                          ┌────┴────┐
  │ Router A│ ══════ Internet ═══════ │ Router B│
  │ 10.0.0.1│                          │ 10.0.0.2│
  └─────────┘                          └─────────┘

Hosts en Madrid acceden a hosts en Barcelona y viceversa
```

```ini
# Router A: /etc/wireguard/wg0.conf
[Interface]
PrivateKey = <key_a>
Address = 10.0.0.1/24
ListenPort = 51820
PostUp = iptables -A FORWARD -i wg0 -j ACCEPT
PostDown = iptables -D FORWARD -i wg0 -j ACCEPT

[Peer]
PublicKey = <pubkey_b>
AllowedIPs = 10.0.0.2/32, 192.168.2.0/24
Endpoint = router-b.empresa.com:51820
PersistentKeepalive = 25
```

```ini
# Router B: /etc/wireguard/wg0.conf
[Interface]
PrivateKey = <key_b>
Address = 10.0.0.2/24
ListenPort = 51820
PostUp = iptables -A FORWARD -i wg0 -j ACCEPT
PostDown = iptables -D FORWARD -i wg0 -j ACCEPT

[Peer]
PublicKey = <pubkey_a>
AllowedIPs = 10.0.0.1/32, 192.168.1.0/24
Endpoint = router-a.empresa.com:51820
PersistentKeepalive = 25
```

Ambos routers necesitan IP forwarding habilitado y las redes locales deben tener rutas para alcanzar la red remota vía el router WireGuard.

### Escenario 3: Múltiples clientes

```ini
# Servidor con varios peers:
[Interface]
PrivateKey = <server_key>
Address = 10.0.0.1/24
ListenPort = 51820
PostUp = iptables -A FORWARD -i wg0 -j ACCEPT; iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
PostDown = iptables -D FORWARD -i wg0 -j ACCEPT; iptables -t nat -D POSTROUTING -o eth0 -j MASQUERADE

# Empleado 1
[Peer]
PublicKey = <pubkey_emp1>
AllowedIPs = 10.0.0.2/32

# Empleado 2
[Peer]
PublicKey = <pubkey_emp2>
AllowedIPs = 10.0.0.3/32

# Empleado 3
[Peer]
PublicKey = <pubkey_emp3>
AllowedIPs = 10.0.0.4/32

# Cada empleado tiene su propia IP en 10.0.0.0/24
# Para revocar acceso: eliminar el bloque [Peer] y recargar
```

---

## 8. WireGuard con NetworkManager y systemd-networkd

### NetworkManager

```bash
# Importar configuración existente
nmcli connection import type wireguard file /etc/wireguard/wg0.conf

# O crear desde cero
nmcli connection add type wireguard con-name wg0 ifname wg0 \
    wireguard.private-key "<private_key>" \
    wireguard.listen-port 51820 \
    ipv4.method manual \
    ipv4.addresses "10.0.0.1/24"

# Agregar peer (más complejo con nmcli, se recomienda editar el archivo)
nmcli connection modify wg0 +wireguard.peer \
    "public-key=<peer_pubkey>, allowed-ips=10.0.0.2/32, endpoint=198.51.100.1:51820"

# Activar
nmcli connection up wg0

# Verificar
nmcli connection show wg0
```

El archivo que NM genera:

```ini
# /etc/NetworkManager/system-connections/wg0.nmconnection
[connection]
id=wg0
type=wireguard
interface-name=wg0

[wireguard]
private-key=oK56DE9Ue9zK76rAc8pBl6opph+1v36lm7cXXsQKrQM=
listen-port=51820

[wireguard-peer.xTIBA5rboUvnH4htodjb6e697QjLERt1NAB4mZqp8Dg=]
endpoint=198.51.100.1:51820
allowed-ips=10.0.0.2/32;

[ipv4]
method=manual
address1=10.0.0.1/24

[ipv6]
method=disabled
```

### systemd-networkd

```ini
# /etc/systemd/network/20-wg0.netdev
[NetDev]
Name=wg0
Kind=wireguard
Description=WireGuard tunnel

[WireGuard]
PrivateKey=oK56DE9Ue9zK76rAc8pBl6opph+1v36lm7cXXsQKrQM=
ListenPort=51820

[WireGuardPeer]
PublicKey=xTIBA5rboUvnH4htodjb6e697QjLERt1NAB4mZqp8Dg=
AllowedIPs=10.0.0.2/32
Endpoint=198.51.100.1:51820
PersistentKeepalive=25
```

```ini
# /etc/systemd/network/25-wg0.network
[Match]
Name=wg0

[Network]
Address=10.0.0.1/24

[Route]
Destination=192.168.2.0/24
# Sin Gateway — se enruta por wg0 basado en AllowedIPs
```

```bash
# Proteger la clave privada en el .netdev
sudo chmod 640 /etc/systemd/network/20-wg0.netdev
sudo chown root:systemd-network /etc/systemd/network/20-wg0.netdev

# Aplicar
sudo networkctl reload
sudo networkctl reconfigure wg0
```

> **Ventaja de systemd-networkd**: la clave privada puede ir en un archivo separado referenciado con `PrivateKeyFile=`, más seguro que inline.

```ini
[WireGuard]
PrivateKeyFile=/etc/wireguard/private.key
ListenPort=51820
```

---

## 9. OpenVPN: visión general

OpenVPN es una VPN basada en SSL/TLS que opera en espacio de usuario. Es más antigua, más compleja, pero también más flexible que WireGuard.

### Arquitectura

```
OpenVPN:                              WireGuard:

┌──────────────────┐                  ┌──────────────────┐
│ Espacio usuario  │                  │ Espacio usuario  │
│                  │                  │                  │
│  openvpn daemon  │                  │  wg (config)     │
│  (proceso)       │                  │  (solo config,   │
│  - TLS handshake │                  │   no datos)      │
│  - Cifrado       │                  │                  │
│  - Compresión    │                  └────────┬─────────┘
│                  │                           │
└────────┬─────────┘                  ┌────────┴─────────┐
         │                            │ Kernel            │
┌────────┴─────────┐                  │                  │
│ Kernel           │                  │ wireguard.ko     │
│                  │                  │ (cifrado,        │
│ tun/tap device   │                  │  encapsulación,  │
│ (solo I/O)       │                  │  todo en kernel) │
└──────────────────┘                  └──────────────────┘

OpenVPN: crypto en userspace          WireGuard: crypto en kernel
→ context switches por paquete        → zero context switches
→ más lento                           → más rápido
```

### Modos de operación

```
TUN mode (L3, recomendado):
  - Solo tráfico IP
  - Más eficiente
  - Similar a WireGuard

TAP mode (L2):
  - Tramas Ethernet completas
  - Permite bridging
  - Broadcast cruza el túnel
  - Más overhead
  - WireGuard NO puede hacer esto
```

### Autenticación

OpenVPN soporta múltiples métodos:

```
Certificados (PKI):
  - Cada cliente tiene certificado + clave
  - CA firma los certificados
  - Más seguro, escalable
  - Más complejo de gestionar
  - Requiere: ca.crt, server.crt, server.key, dh.pem, ta.key

Pre-shared key (estática):
  - Una clave compartida entre ambos extremos
  - Simple pero no escala (solo 2 peers)

Username/password:
  - Sobre TLS, con plugin de autenticación
  - Integrable con LDAP, RADIUS
```

### Configuración básica (ejemplo)

```
# /etc/openvpn/server.conf (servidor)
port 1194
proto udp
dev tun
ca ca.crt
cert server.crt
key server.key
dh dh2048.pem
server 10.8.0.0 255.255.255.0
push "route 192.168.1.0 255.255.255.0"
push "dhcp-option DNS 192.168.1.1"
keepalive 10 120
cipher AES-256-GCM
persist-key
persist-tun
status openvpn-status.log
verb 3
```

```
# /etc/openvpn/client.conf (cliente)
client
dev tun
proto udp
remote vpn.empresa.com 1194
resolv-retry infinite
nobind
persist-key
persist-tun
ca ca.crt
cert client.crt
key client.key
cipher AES-256-GCM
verb 3
```

```bash
# Gestión con systemd
sudo systemctl start openvpn-server@server
sudo systemctl enable openvpn-server@server
```

---

## 10. WireGuard vs OpenVPN

```
┌────────────────────┬──────────────────────┬──────────────────────┐
│ Criterio           │ WireGuard            │ OpenVPN              │
├────────────────────┼──────────────────────┼──────────────────────┤
│ Ubicación          │ Kernel               │ Userspace            │
│ Líneas de código   │ ~4,000               │ ~100,000             │
│ Protocolo          │ UDP                  │ UDP o TCP            │
│ Rendimiento        │ Excelente            │ Bueno                │
│ Latencia           │ Muy baja             │ Mayor (userspace)    │
│ Capa               │ L3 (TUN solo)        │ L2 (TAP) o L3 (TUN) │
│ Cifrado            │ ChaCha20-Poly1305    │ Configurable (AES,   │
│                    │ (fijo, no negociable)│ ChaCha20, etc.)      │
│ Autenticación      │ Claves públicas      │ Certificados PKI,    │
│                    │                      │ user/pass, claves    │
│ Configuración      │ ~15 líneas           │ ~30-50 líneas        │
│ Gestión de users   │ Agregar/quitar peer  │ Revocar certificado  │
│ NAT traversal      │ Nativo (UDP)         │ TCP posible (p.443)  │
│ Bypass firewalls   │ Solo UDP             │ TCP/443 imita HTTPS  │
│ IP estática        │ Sí (cada peer)       │ Configurable         │
│ Asignación dinámica│ No (manual)          │ Sí (--server pool)   │
│ Roaming            │ Sí (actualiza endpt) │ Reconecta            │
│ Auditoría          │ Auditable (pequeño)  │ Más difícil          │
│ Madurez            │ 2018+ (kernel 5.6)   │ 2001+ (20+ años)    │
│ Soporte OS         │ Linux,Win,Mac,*BSD,  │ Linux,Win,Mac,*BSD,  │
│                    │ iOS, Android         │ iOS, Android         │
└────────────────────┴──────────────────────┴──────────────────────┘
```

### Cuándo usar cada uno

```
Usa WireGuard cuando:
  ├── Rendimiento es prioridad
  ├── Configuración simple
  ├── Site-to-site o remote access básico
  ├── Infraestructura Linux moderna
  └── No necesitas TAP (L2)

Usa OpenVPN cuando:
  ├── Necesitas TCP (bypass firewalls que bloquean UDP)
  ├── Necesitas TAP/L2 (bridging)
  ├── Necesitas autenticación PKI corporativa con CA
  ├── Integración con LDAP/RADIUS
  ├── Infraestructura legacy que ya lo usa
  └── Necesitas asignación dinámica de IPs del pool
```

---

## 11. Diagnóstico de túneles VPN

### WireGuard

```bash
# Estado general
sudo wg show

# Estado detallado de un túnel
sudo wg show wg0

# Campos importantes:
# latest handshake: debe ser reciente (<2 min si hay tráfico)
# transfer: rx/tx debe incrementar si hay tráfico
# endpoint: dónde cree que está el peer

# Si "latest handshake" dice "none":
# → No se ha completado el handshake
# → Claves incorrectas, firewall, endpoint incorrecto
```

```bash
# Ver la interfaz
ip addr show wg0
ip link show wg0

# Ver rutas instaladas por WireGuard
ip route show dev wg0

# Test de conectividad
ping -I wg0 10.0.0.2

# Capturar tráfico encapsulado (ve los paquetes UDP cifrados)
sudo tcpdump -i eth0 -nn udp port 51820

# Capturar tráfico dentro del túnel (ve los paquetes desencapsulados)
sudo tcpdump -i wg0 -nn
```

```bash
# Logs del kernel (debugging)
echo "module wireguard +p" | sudo tee /sys/kernel/debug/dynamic_debug/control
dmesg | grep wireguard
# Desactivar debug:
echo "module wireguard -p" | sudo tee /sys/kernel/debug/dynamic_debug/control
```

### OpenVPN

```bash
# Logs
journalctl -u openvpn-server@server -f

# Estado de conexiones (si configurado status file)
cat /var/log/openvpn/openvpn-status.log

# Ver la interfaz del túnel
ip addr show tun0

# Management interface (si habilitada)
telnet localhost 7505
```

### Flujo de diagnóstico

```
¿El túnel WireGuard funciona?
│
├── sudo wg show → ¿Aparece el peer?
│   └── NO → Configuración incorrecta (claves, archivo .conf)
│
├── ¿latest handshake es reciente?
│   └── NO → ¿Firewall bloquea UDP 51820?
│            ¿Endpoint correcto?
│            ¿Claves corresponden (pub_A ↔ priv_A)?
│
├── ¿transfer muestra bytes rx/tx?
│   └── NO rx → El peer no envía (su config, su firewall)
│   └── NO tx → Rutas incorrectas en AllowedIPs
│
├── ping 10.0.0.2 funciona?
│   └── NO → ¿Rutas correctas? ip route get 10.0.0.2
│            ¿AllowedIPs incluye ese destino?
│
└── ping 192.168.2.10 (red remota) funciona?
    └── NO → ¿IP forwarding habilitado en el peer remoto?
             ¿AllowedIPs incluye 192.168.2.0/24?
             ¿iptables FORWARD permite el tráfico?
             ¿NAT/MASQUERADE configurado?
```

---

## 12. Errores comunes

### Error 1: Claves pública y privada no corresponden entre peers

```
PROBLEMA:
# Peer A configura la clave pública de B
# Pero la clave pública no corresponde a la privada de B
# Resultado: handshake nunca se completa

sudo wg show wg0
# latest handshake: (none)    ← NUNCA se completó

VERIFICAR:
# En Peer B, generar la pública a partir de su privada:
echo "<private_key_B>" | wg pubkey
# El resultado debe coincidir con lo que Peer A tiene configurado

SOLUCIÓN:
# Regenerar claves si es necesario
# En Peer B:
wg genkey | tee /etc/wireguard/privatekey | wg pubkey > /etc/wireguard/publickey
# Compartir la nueva clave pública con Peer A
```

### Error 2: AllowedIPs no incluye la subred remota

```
PROBLEMA:
# Peer A quiere acceder a 192.168.2.0/24 (red detrás de Peer B)
# Pero en la config de A, AllowedIPs del peer B solo tiene:
AllowedIPs = 10.0.0.2/32

# ping 10.0.0.2 funciona (IP del túnel)
# ping 192.168.2.10 NO funciona (red remota)

CORRECTO:
# En Peer A, agregar la subred remota:
AllowedIPs = 10.0.0.2/32, 192.168.2.0/24

# En Peer B, también necesita:
# 1. IP forwarding habilitado
# 2. AllowedIPs para Peer A debe incluir la subred de A
# 3. iptables debe permitir FORWARD
```

### Error 3: PersistentKeepalive omitido detrás de NAT

```
PROBLEMA:
# El cliente está detrás de NAT (red doméstica)
# Establece conexión con el servidor
# Funciona 2 minutos → deja de funcionar

# El mapping NAT expira (típicamente 60-120 segundos)
# El servidor ya no puede enviar paquetes de vuelta

CORRECTO:
# En el cliente (detrás de NAT), agregar:
[Peer]
PersistentKeepalive = 25
# Envía un keepalive cada 25 segundos
# Mantiene el mapping NAT vivo
```

### Error 4: Olvidar IP forwarding en el gateway VPN

```
PROBLEMA:
# Servidor WireGuard es gateway para la red interna
# Clientes pueden hacer ping al servidor (10.0.0.1)
# Pero NO pueden alcanzar la red interna (192.168.1.0/24)

CAUSA:
# IP forwarding deshabilitado:
cat /proc/sys/net/ipv4/ip_forward
# 0

CORRECTO:
# Habilitar IP forwarding
echo 1 | sudo tee /proc/sys/net/ipv4/ip_forward

# Persistente:
echo "net.ipv4.ip_forward = 1" | sudo tee /etc/sysctl.d/99-forward.conf
sudo sysctl -p /etc/sysctl.d/99-forward.conf

# Y agregar NAT/MASQUERADE si los hosts internos
# no tienen ruta de vuelta al túnel:
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
iptables -A FORWARD -i wg0 -j ACCEPT
```

### Error 5: MTU incorrecto causa paquetes que no llegan

```
PROBLEMA:
# ping funciona (paquetes pequeños)
# SSH funciona (paquetes pequeños)
# Descarga de archivos se cuelga (paquetes grandes)
# HTTPS no carga (TLS handshake son paquetes grandes)

CAUSA:
# MTU del túnel demasiado grande
# Overhead de WireGuard: ~60-80 bytes
# Si eth0 tiene MTU 1500, wg0 debería tener ≤1420

CORRECTO:
# WireGuard wg-quick configura MTU 1420 automáticamente
# Si configuraste manualmente, verificar:
ip link show wg0 | grep mtu

# Ajustar si es necesario:
sudo ip link set wg0 mtu 1420

# En wg-quick:
[Interface]
MTU = 1420

# Si estás en una red con MTU menor (PPPoE = 1492):
# MTU del túnel = MTU de la red - 80
# PPPoE: 1492 - 80 = 1412
```

---

## 13. Cheatsheet

```
WIREGUARD — CLAVES
──────────────────────────────────────────────
wg genkey                     Generar clave privada
wg pubkey < privatekey        Derivar clave pública
wg genpsk                     Generar pre-shared key
wg genkey | tee priv | wg pubkey > pub   Todo en uno

WIREGUARD — WG-QUICK
──────────────────────────────────────────────
wg-quick up wg0               Levantar túnel
wg-quick down wg0             Bajar túnel
systemctl enable wg-quick@wg0 Habilitar al boot
systemctl start wg-quick@wg0  Iniciar con systemd

Config: /etc/wireguard/wg0.conf
Permisos: chmod 600 /etc/wireguard/wg0.conf

WIREGUARD — ESTADO
──────────────────────────────────────────────
wg show                       Estado de todos los túneles
wg show wg0                   Estado de wg0
wg showconf wg0               Mostrar config activa

Verificar:
  latest handshake: < 2 min (si hay tráfico)
  transfer: bytes rx/tx incrementando
  endpoint: IP:puerto del peer

WIREGUARD — ARCHIVO .CONF
──────────────────────────────────────────────
[Interface]
  PrivateKey = ...            Clave privada
  Address = 10.0.0.1/24      IP del túnel
  ListenPort = 51820         Puerto (servidor)
  DNS = 1.1.1.1              DNS (cliente)
  MTU = 1420                 MTU (opcional)
  PostUp = ...               Comando post-activación
  PostDown = ...             Comando post-desactivación

[Peer]
  PublicKey = ...             Clave pública del peer
  PresharedKey = ...          PSK (opcional)
  AllowedIPs = 10.0.0.2/32   IPs enrutadas por este peer
  Endpoint = IP:puerto        Dónde contactar al peer
  PersistentKeepalive = 25    Keepalive (si NAT)

ALLOWEDIPS COMUNES
──────────────────────────────────────────────
10.0.0.2/32                   Solo el peer
10.0.0.0/24                   Subred del túnel
10.0.0.0/24, 192.168.2.0/24  Túnel + red remota
0.0.0.0/0, ::/0              Full tunnel (todo)

DIAGNÓSTICO
──────────────────────────────────────────────
wg show                       Estado y handshake
ip route get 10.0.0.2         Verificar ruta
ping -I wg0 10.0.0.2          Ping por el túnel
tcpdump -i wg0                Tráfico descifrado
tcpdump -i eth0 udp port 51820   Tráfico cifrado

REQUISITOS PARA ROUTING
──────────────────────────────────────────────
sysctl net.ipv4.ip_forward=1  IP forwarding
iptables -A FORWARD -i wg0 -j ACCEPT
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE

OPENVPN — BÁSICO
──────────────────────────────────────────────
systemctl start openvpn-server@server    Iniciar
systemctl start openvpn-client@client    Cliente
journalctl -u openvpn-server@server      Logs
ip addr show tun0                        Interfaz
```

---

## 14. Ejercicios

### Ejercicio 1: Generar claves y entender la criptografía

1. Genera un par de claves WireGuard (privada + pública) y un PSK.
2. Genera un segundo par de claves (simulando un peer remoto).
3. Escribe los dos archivos `wg0.conf` (uno por peer) para una VPN punto a punto:
   - Peer A: 10.0.0.1/24, escucha en puerto 51820
   - Peer B: 10.0.0.2/24, endpoint de A es 203.0.113.1:51820
   - Usa el PSK generado
4. Sin activar la VPN: ¿qué pasaría si intercambiaras accidentalmente las claves privadas (la de A en B y viceversa)?

### Ejercicio 2: Diseñar VPN para una empresa

Una empresa tiene:
- Oficina central en Madrid (192.168.1.0/24, IP pública: 81.0.0.1)
- Sucursal en Barcelona (192.168.2.0/24, IP pública: 82.0.0.2)
- 10 empleados remotos que trabajan desde casa (detrás de NAT)

Diseña la solución WireGuard:
1. ¿Qué topología usarías: hub-and-spoke (todos conectan a Madrid) o mesh (todos conectan entre sí)?
2. Escribe el `wg0.conf` del servidor en Madrid, incluyendo la sucursal Barcelona y 2 empleados remotos de ejemplo.
3. ¿Qué `AllowedIPs` necesita cada peer?
4. ¿Qué peers necesitan `PersistentKeepalive` y por qué?
5. ¿Qué configuración de iptables necesita el servidor en Madrid?

### Ejercicio 3: Diagnosticar un túnel roto

Un administrador reporta que el túnel WireGuard dejó de funcionar. La salida de `wg show wg0` es:

```
interface: wg0
  public key: YDcxq2lLth3IP+L45BKGOD4yd5RJHSqGfOEzMGCRSzY=
  private key: (hidden)
  listening port: 51820

peer: xTIBA5rboUvnH4htodjb6e697QjLERt1NAB4mZqp8Dg=
  endpoint: 198.51.100.1:51820
  allowed ips: 10.0.0.2/32
  latest handshake: 3 hours, 42 minutes, 15 seconds ago
  transfer: 156.24 MiB received, 89.47 MiB sent
```

Y `ping 10.0.0.2` devuelve `100% packet loss`.

1. El handshake fue hace casi 4 horas. ¿Eso es normal si había tráfico activo?
2. Los contadores de transfer muestran datos. ¿El túnel funcionó alguna vez?
3. Lista al menos 4 posibles causas del fallo, ordenadas de más a menos probable.
4. ¿Qué comandos ejecutarías para diagnosticar cada causa?
5. Si el peer remoto cambió de IP pública (era 198.51.100.1, ahora es 198.51.100.99), ¿WireGuard se adapta automáticamente? ¿En qué dirección?

**Pregunta de reflexión**: WireGuard no tiene concepto de "revocar acceso" como OpenVPN con CRL (Certificate Revocation List). Para quitar acceso a un empleado, debes eliminar su bloque `[Peer]` de la configuración y recargar. Con 100 empleados, esto se vuelve tedioso. ¿Cómo automatizarías la gestión de peers? ¿Qué herramientas o scripts crearías? ¿Es esta una razón válida para preferir OpenVPN en entornos enterprise?

---

> **Siguiente capítulo**: C05 — Firewall (Netfilter e iptables, nftables y firewalld)
