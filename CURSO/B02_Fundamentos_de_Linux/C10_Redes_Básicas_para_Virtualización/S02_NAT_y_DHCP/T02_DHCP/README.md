# DHCP (Dynamic Host Configuration Protocol)

## Índice

1. [El problema: asignar IPs a mano no escala](#1-el-problema-asignar-ips-a-mano-no-escala)
2. [Qué es DHCP](#2-qué-es-dhcp)
3. [El proceso DORA paso a paso](#3-el-proceso-dora-paso-a-paso)
4. [Qué entrega el servidor DHCP](#4-qué-entrega-el-servidor-dhcp)
5. [El lease: alquiler temporal de la IP](#5-el-lease-alquiler-temporal-de-la-ip)
6. [Renovación y expiración del lease](#6-renovación-y-expiración-del-lease)
7. [DHCP en libvirt: dnsmasq](#7-dhcp-en-libvirt-dnsmasq)
8. [Reservas DHCP: IP fija por MAC](#8-reservas-dhcp-ip-fija-por-mac)
9. [Ver y gestionar leases](#9-ver-y-gestionar-leases)
10. [IP estática vs DHCP: cuándo usar cada una](#10-ip-estática-vs-dhcp-cuándo-usar-cada-una)
11. [Capturar DORA con tcpdump](#11-capturar-dora-con-tcpdump)
12. [DHCP y NetworkManager/systemd-networkd](#12-dhcp-y-networkmanagersystemd-networkd)
13. [Errores comunes](#13-errores-comunes)
14. [Cheatsheet](#14-cheatsheet)
15. [Ejercicios](#15-ejercicios)

---

## 1. El problema: asignar IPs a mano no escala

Cada dispositivo en una red necesita al menos cuatro parámetros configurados:

```
IP:       192.168.122.100
Máscara:  255.255.255.0
Gateway:  192.168.122.1
DNS:      192.168.122.1
```

Configurar esto manualmente en 3 VMs es tolerable. En 30 VMs es tedioso. En 300 es inviable. Y hay problemas adicionales:

- **Conflictos de IP**: si asignas la misma IP a dos dispositivos, ambos pierden conectividad.
- **Cambios de configuración**: si el gateway cambia de .1 a .254, debes actualizar cada dispositivo.
- **Rotación de dispositivos**: las VMs se crean y destruyen constantemente — gestionar manualmente un pool de IPs es propenso a errores.

DHCP resuelve todos estos problemas: un servidor central asigna IPs automáticamente, evita duplicados, y propaga cambios de configuración.

---

## 2. Qué es DHCP

DHCP (Dynamic Host Configuration Protocol) es un protocolo cliente-servidor que asigna automáticamente configuración de red a los dispositivos que se conectan. Opera en las capas 4/7 del modelo OSI, usando UDP:

| Componente | Puerto UDP | Rol |
|-----------|-----------|-----|
| Servidor DHCP | 67 | Escucha peticiones, asigna IPs |
| Cliente DHCP | 68 | Solicita configuración de red |

### ¿Por qué UDP y no TCP?

TCP requiere una conexión establecida, que a su vez requiere una IP. Pero el cliente DHCP **no tiene IP** — precisamente por eso está pidiendo una. UDP no necesita conexión previa: el cliente envía un datagrama broadcast desde 0.0.0.0 (sin IP origen) y el servidor responde.

### Participantes en un entorno de virtualización

```
┌── HOST ──────────────────────────────────────┐
│                                               │
│  dnsmasq (servidor DHCP de libvirt)           │
│  Escucha en virbr0 (192.168.122.1)           │
│  Pool: 192.168.122.2 – 192.168.122.254      │
│                                               │
│  ┌─── VM1 ──────┐    ┌─── VM2 ──────┐       │
│  │ Cliente DHCP  │    │ Cliente DHCP  │       │
│  │ (dhclient o   │    │ (dhclient o   │       │
│  │  systemd-     │    │  systemd-     │       │
│  │  networkd)    │    │  networkd)    │       │
│  │               │    │               │       │
│  │ Obtiene:      │    │ Obtiene:      │       │
│  │ .122.100      │    │ .122.101      │       │
│  └───────────────┘    └───────────────┘       │
└───────────────────────────────────────────────┘
```

---

## 3. El proceso DORA paso a paso

DHCP usa un intercambio de 4 mensajes conocido como **DORA**: Discover, Offer, Request, Acknowledge.

### Diagrama temporal

```
    Cliente (sin IP)                     Servidor DHCP (192.168.122.1)
         │                                        │
    ┌────┤  1. DHCP DISCOVER                      │
    │    │──── broadcast ────────────────────────→ │
    │    │  src=0.0.0.0:68  dst=255.255.255.255:67│
    │    │  "¿Hay algún servidor DHCP?"            │
    │    │                                        │
    │    │  2. DHCP OFFER                          │
    │    │ ←──────────────────────────────────────│
    │    │  "Te ofrezco 192.168.122.100,           │
    │    │   gateway .1, DNS .1, lease 3600s"      │
    │    │                                        │
    │    │  3. DHCP REQUEST                        │
    │    │──── broadcast ────────────────────────→ │
    │    │  "Acepto 192.168.122.100"               │
    │    │  (broadcast porque otros servidores     │
    │    │   DHCP deben saber que rechacé          │
    │    │   sus ofertas)                           │
    │    │                                        │
    │    │  4. DHCP ACK                            │
    │    │ ←──────────────────────────────────────│
    │    │  "Confirmado. Lease de 3600s.            │
    └────┤   Tu IP es 192.168.122.100."            │
         │                                        │
    Cliente configura su interfaz:                │
    ip=192.168.122.100/24                         │
    gw=192.168.122.1                              │
    dns=192.168.122.1                             │
```

### Detalles de cada paso

**1. DISCOVER** — el cliente grita al vacío

El cliente no sabe si hay un servidor DHCP ni dónde está. Envía un broadcast Ethernet (dst MAC: ff:ff:ff:ff:ff:ff) y un broadcast IP (dst: 255.255.255.255). Como no tiene IP propia, usa 0.0.0.0 como origen.

El mensaje incluye la MAC del cliente — el servidor la necesitará para responder y para posibles reservas.

**2. OFFER** — el servidor propone

El servidor selecciona una IP libre de su pool y se la ofrece al cliente. La oferta incluye:
- IP propuesta
- Máscara de red
- Gateway
- DNS
- Duración del lease

Si hay múltiples servidores DHCP, cada uno envía su propio OFFER. El cliente elige uno (normalmente el primero que llega).

**3. REQUEST** — el cliente acepta

El cliente anuncia (en broadcast) qué oferta acepta. Hace broadcast por dos razones:
- El servidor elegido confirma la asignación.
- Los demás servidores (si los hay) retiran sus ofertas y liberan las IPs propuestas.

**4. ACK** — el servidor confirma

El servidor confirma la asignación. El cliente configura su interfaz de red y empieza a usar la IP.

Si el servidor detecta que la IP ya está en uso (hace un ARP para verificar), envía un **NACK** y el proceso reinicia.

### ¿Y si no hay servidor DHCP?

Si el cliente no recibe un OFFER después de varios intentos, se auto-asigna una IP link-local (169.254.x.x) mediante APIPA. Como vimos en T03 de S01, esta IP no es funcional para comunicación general.

---

## 4. Qué entrega el servidor DHCP

DHCP no solo asigna una IP — entrega un paquete completo de configuración de red a través de **opciones DHCP** (DHCP options):

### Opciones estándar

| Opción | Código | Qué configura | Ejemplo |
|--------|--------|---------------|---------|
| Subnet Mask | 1 | Máscara de red | 255.255.255.0 |
| Router | 3 | Gateway por defecto | 192.168.122.1 |
| DNS Servers | 6 | Servidores DNS | 192.168.122.1, 8.8.8.8 |
| Domain Name | 15 | Dominio de búsqueda | lab.local |
| Lease Time | 51 | Duración del alquiler | 3600 (1 hora) |
| DHCP Server ID | 54 | IP del servidor DHCP | 192.168.122.1 |
| NTP Servers | 42 | Servidores de hora | 192.168.122.1 |
| Hostname | 12 | Nombre del host | fedora-vm |

### En libvirt

La red default de libvirt entrega:
- **IP**: del pool 192.168.122.2–254
- **Máscara**: 255.255.255.0 (/24)
- **Gateway**: 192.168.122.1
- **DNS**: 192.168.122.1 (dnsmasq del host actúa como DNS forwarder)

```bash
# Ver qué DNS entregó DHCP (dentro de la VM):
cat /etc/resolv.conf
# nameserver 192.168.122.1

# Verificar que el DNS funciona:
dig google.com @192.168.122.1
```

El dnsmasq de libvirt reenvía las consultas DNS al DNS configurado en el host — no resuelve por sí mismo. Si el host usa 8.8.8.8 como DNS, las VMs también lo usarán indirectamente.

---

## 5. El lease: alquiler temporal de la IP

Las IPs asignadas por DHCP no son permanentes — se "alquilan" por un período definido por el servidor. Esto permite reutilizar IPs de dispositivos que ya no están conectados.

### Anatomía de un lease

```
lease {
  interface "eth0";
  fixed-address 192.168.122.100;
  option subnet-mask 255.255.255.0;
  option routers 192.168.122.1;
  option domain-name-servers 192.168.122.1;
  option dhcp-lease-time 3600;
  renew 2026-03-19 15:30:00;
  rebind 2026-03-19 16:07:30;
  expire 2026-03-19 16:15:00;
}
```

| Campo | Significado |
|-------|------------|
| `fixed-address` | IP asignada |
| `dhcp-lease-time` | Duración total del lease en segundos |
| `renew` | Momento en que el cliente intentará renovar (50% del lease) |
| `rebind` | Si renew falla, momento del segundo intento (87.5% del lease) |
| `expire` | Momento en que la IP expira y se libera |

### Tiempos de lease típicos

| Contexto | Lease time | Razón |
|----------|-----------|-------|
| Red doméstica | 24 horas | Los dispositivos suelen estar conectados mucho tiempo |
| Red de oficina | 8 horas | Jornada laboral |
| Cafetería/WiFi público | 1-2 horas | Alta rotación de dispositivos |
| libvirt default | 1 hora | Las VMs pueden crearse/destruirse frecuentemente |
| Red de lab | 1-2 horas | Entorno dinámico |

### ¿Qué pasa si una VM se apaga sin liberar el lease?

El lease sigue activo en el servidor. La IP queda "reservada" hasta que el lease expire. Si la VM vuelve a arrancar antes de que expire, pedirá la misma IP (DHCP REQUEST con la IP anterior) y normalmente la obtendrá.

Si otra VM se crea mientras la primera está apagada y el lease aún no expiró, la nueva VM recibirá una IP diferente — el servidor no reasigna una IP con lease activo.

---

## 6. Renovación y expiración del lease

### Línea temporal de un lease

```
Tiempo 0          50%            87.5%         100%
   │               │               │              │
   ├───────────────┼───────────────┼──────────────┤
   │               │               │              │
   │  IP activa    │  RENEW        │  REBIND      │  EXPIRE
   │  funcionando  │  unicast al   │  broadcast   │  IP se
   │  normalmente  │  servidor     │  a cualquier │  pierde
   │               │  original     │  servidor    │
```

**50% (T1 — Renew)**: el cliente envía un REQUEST **unicast** al servidor que le dio el lease. Si el servidor responde ACK, el lease se extiende otro período completo.

**87.5% (T2 — Rebind)**: si el renew falló (servidor caído, red cambiada), el cliente envía un REQUEST **broadcast** a cualquier servidor DHCP en la red. Si otro servidor responde, le asigna una nueva IP.

**100% (Expire)**: si nadie respondió, el cliente pierde la IP. Debe reiniciar el proceso DORA desde cero. La interfaz de red queda sin IP funcional.

### Renovar manualmente

```bash
# Dentro de una VM o en el host:

# Liberar la IP actual (envía DHCP Release al servidor)
sudo dhclient -r eth0

# Solicitar una nueva IP (proceso DORA completo)
sudo dhclient eth0

# Ver el estado DHCP con NetworkManager:
nmcli connection show "Wired connection 1" | grep DHCP

# Forzar reconexión con NetworkManager:
nmcli connection down "Wired connection 1" && \
nmcli connection up "Wired connection 1"
```

### ¿Cambia la IP al renovar?

Normalmente **no**. El cliente solicita la misma IP que tenía, y el servidor la confirma si sigue disponible. La IP cambia solo si:
- El lease expiró completamente.
- El servidor fue reconfigurado (rango diferente).
- Otro dispositivo tomó esa IP mientras el cliente estuvo desconectado.

---

## 7. DHCP en libvirt: dnsmasq

Libvirt no implementa DHCP desde cero — usa **dnsmasq**, un servidor ligero que combina DHCP y DNS forwarding en un solo proceso.

### dnsmasq: el servidor detrás de libvirt

Cuando activas una red virtual con libvirt, se lanza un proceso dnsmasq dedicado a esa red:

```bash
# Ver el proceso dnsmasq de la red default
ps aux | grep dnsmasq
# nobody  1234  dnsmasq --conf-file=/var/lib/libvirt/dnsmasq/default.conf
#                       --dhcp-range=192.168.122.2,192.168.122.254,255.255.255.0
#                       --listen-address=192.168.122.1
#                       --dhcp-leasefile=/var/lib/libvirt/dnsmasq/default.leases
```

### Archivos de configuración y leases

| Archivo | Contenido |
|---------|-----------|
| `/var/lib/libvirt/dnsmasq/default.conf` | Configuración generada por libvirt (no editar manualmente) |
| `/var/lib/libvirt/dnsmasq/default.leases` | Leases activos (formato texto) |
| `/var/lib/libvirt/dnsmasq/default.addnhosts` | Entradas /etc/hosts adicionales para la red |

### Formato del archivo de leases

```bash
cat /var/lib/libvirt/dnsmasq/default.leases
# 1710864215 52:54:00:ab:cd:ef 192.168.122.100 fedora-vm 01:52:54:00:ab:cd:ef
# │          │                 │                │          │
# │          │                 │                │          └── client-id
# │          │                 │                └── hostname
# │          │                 └── IP asignada
# │          └── MAC address
# └── timestamp de expiración (epoch)
```

Para convertir el timestamp a fecha legible:

```bash
date -d @1710864215
# Tue Mar 19 16:30:15 CET 2026
```

### Cada red tiene su propio dnsmasq

Si creas múltiples redes virtuales, cada una tiene su propio proceso dnsmasq, su propio archivo de leases, y su propio pool de IPs:

```bash
ps aux | grep dnsmasq | grep -v grep
# dnsmasq --conf-file=.../default.conf --listen-address=192.168.122.1  ← red default
# dnsmasq --conf-file=.../lab-net.conf --listen-address=10.0.1.1       ← red lab-net
```

### DNS integrado en dnsmasq

Dnsmasq no solo asigna IPs — también resuelve nombres para las VMs. Si una VM se llama "fedora-vm", dnsmasq registra el nombre y otras VMs en la misma red pueden resolverlo:

```bash
# Desde otra VM en la misma red:
ping fedora-vm
# PING fedora-vm (192.168.122.100) ...

# Esto funciona porque dnsmasq registra hostname → IP
# cuando asigna el lease DHCP
```

---

## 8. Reservas DHCP: IP fija por MAC

Una reserva DHCP (o "static lease") garantiza que una VM siempre reciba la misma IP, basándose en su dirección MAC.

### ¿Por qué reservar?

- **Port forwarding**: si configuras DNAT hacia 192.168.122.100 y la VM obtiene .101 por DHCP, el forwarding deja de funcionar.
- **Scripts de automatización**: `ssh user@192.168.122.100` deja de funcionar si la IP cambia.
- **Referencia estable**: saber siempre en qué IP está cada VM.

### Método 1: virsh net-update (en caliente)

```bash
# 1. Obtener la MAC de la VM
virsh domiflist fedora-vm
# Interface  Type    Source   Model    MAC
# vnet0      network default virtio   52:54:00:ab:cd:ef

# 2. Crear la reserva
virsh net-update default add ip-dhcp-host \
    '<host mac="52:54:00:ab:cd:ef" name="fedora-vm" ip="192.168.122.100"/>' \
    --live --config

# --live: aplicar inmediatamente (sin reiniciar la red)
# --config: guardar en la configuración persistente
```

### Método 2: virsh net-edit (edición del XML)

```bash
virsh net-edit default
```

Dentro de la sección `<dhcp>`, añadir:

```xml
<ip address='192.168.122.1' netmask='255.255.255.0'>
  <dhcp>
    <range start='192.168.122.2' end='192.168.122.254'/>
    <host mac='52:54:00:ab:cd:ef' name='fedora-vm' ip='192.168.122.100'/>
    <host mac='52:54:00:11:22:33' name='debian-db' ip='192.168.122.101'/>
    <host mac='52:54:00:44:55:66' name='centos-web' ip='192.168.122.102'/>
  </dhcp>
</ip>
```

Después de editar:
```bash
# Reiniciar la red para aplicar cambios (si no usaste --live)
virsh net-destroy default && virsh net-start default

# Las VMs necesitarán renovar su lease:
# Dentro de cada VM: sudo dhclient -r eth0 && sudo dhclient eth0
# O simplemente reiniciar la VM
```

### Eliminar una reserva

```bash
virsh net-update default delete ip-dhcp-host \
    '<host mac="52:54:00:ab:cd:ef" name="fedora-vm" ip="192.168.122.100"/>' \
    --live --config
```

### Reserva vs IP estática manual

| Característica | Reserva DHCP | IP estática |
|---------------|-------------|------------|
| Configuración | En el servidor (virsh net-update) | En cada VM (/etc/network/interfaces) |
| Gateway/DNS | Entregados automáticamente | Configurados manualmente |
| Cambios de red | Se propagan automáticamente | Hay que actualizar cada VM |
| IP garantizada | Sí (por MAC) | Sí (configurada a mano) |
| Funciona sin DHCP server | No | Sí |

La reserva DHCP combina lo mejor de ambos mundos: IP predecible + configuración centralizada.

---

## 9. Ver y gestionar leases

### Desde el host

```bash
# Ver leases activos de una red virtual
virsh net-dhcp-leases default
# Expiry               MAC                Protocol  IP address         Hostname
# 2026-03-19 16:30:15  52:54:00:ab:cd:ef  ipv4      192.168.122.100/24  fedora-vm

# Ver el archivo de leases directamente
cat /var/lib/libvirt/dnsmasq/default.leases

# Ver configuración completa de la red (incluye reservas)
virsh net-dumpxml default
```

### Desde la VM

```bash
# Ver la configuración DHCP recibida (NetworkManager)
nmcli device show eth0 | grep -i dhcp
# DHCP4.OPTION[1]:  dhcp-lease-time = 3600
# DHCP4.OPTION[2]:  dhcp-server-identifier = 192.168.122.1
# DHCP4.OPTION[3]:  domain-name-servers = 192.168.122.1
# DHCP4.OPTION[4]:  ip-address = 192.168.122.100
# DHCP4.OPTION[5]:  routers = 192.168.122.1
# DHCP4.OPTION[6]:  subnet-mask = 255.255.255.0

# Ver lease actual (dhclient)
cat /var/lib/dhclient/dhclient*.leases 2>/dev/null

# Ver lease actual (systemd-networkd)
networkctl status eth0

# Ver configuración de red resultante
ip addr show eth0
ip route show default
cat /etc/resolv.conf
```

### Asociar una IP a una VM sin virsh net-dhcp-leases

Si `virsh net-dhcp-leases` no muestra la VM (puede pasar si el lease expiró o si la VM tiene IP estática), alternativas:

```bash
# ARP: ver qué MACs están activas en la red
arp -n | grep 192.168.122

# nmap: escanear la red (requiere nmap)
sudo nmap -sn 192.168.122.0/24

# Consola de la VM
virsh console fedora-vm
# (dentro de la VM) ip addr show
```

---

## 10. IP estática vs DHCP: cuándo usar cada una

### En el host

| Situación | Recomendación |
|-----------|--------------|
| Desktop/laptop doméstico | DHCP (el router asigna) |
| Servidor de producción | IP estática (debe ser predecible) |
| Host de virtualización | IP estática en LAN, DHCP en virbr0 no aplica (es el servidor) |

### En las VMs

| Situación | Recomendación |
|-----------|--------------|
| VM de desarrollo general | DHCP (default de libvirt) |
| VM que ofrece servicios (web, DB) | Reserva DHCP (predecible + centralizada) |
| VM en lab temporal (se destruye pronto) | DHCP (no importa la IP) |
| VM aislada sin red libvirt | IP estática (no hay servidor DHCP) |

### Configurar IP estática dentro de una VM

Si necesitas IP estática sin depender de DHCP:

```bash
# Con NetworkManager (Fedora, RHEL, Ubuntu desktop)
nmcli connection modify "Wired connection 1" \
    ipv4.method manual \
    ipv4.addresses 192.168.122.200/24 \
    ipv4.gateway 192.168.122.1 \
    ipv4.dns 192.168.122.1

nmcli connection up "Wired connection 1"

# Verificar
ip addr show eth0
ip route show default
```

```bash
# Con systemd-networkd (Debian minimal, Fedora CoreOS, servers)
cat > /etc/systemd/network/10-static.network << 'EOF'
[Match]
Name=eth0

[Network]
Address=192.168.122.200/24
Gateway=192.168.122.1
DNS=192.168.122.1
EOF

sudo systemctl restart systemd-networkd
```

```bash
# Con /etc/network/interfaces (Debian/Ubuntu server clásico)
cat > /etc/network/interfaces << 'EOF'
auto eth0
iface eth0 inet static
    address 192.168.122.200/24
    gateway 192.168.122.1
    dns-nameservers 192.168.122.1
EOF

sudo systemctl restart networking
```

---

## 11. Capturar DORA con tcpdump

Observar el proceso DORA con tcpdump es una excelente forma de entender DHCP en la práctica.

### Captura desde el host

```bash
# Terminal 1: capturar tráfico DHCP en el bridge de libvirt
sudo tcpdump -i virbr0 -n -vv port 67 or port 68
```

```bash
# Terminal 2: forzar un ciclo DORA completo (dentro de la VM)
sudo dhclient -r eth0    # liberar IP actual
sudo dhclient eth0       # solicitar nueva IP
```

### Salida esperada (simplificada)

```
# DISCOVER
IP 0.0.0.0.68 > 255.255.255.255.67: BOOTP/DHCP, Request
  Client-Ethernet-Address 52:54:00:ab:cd:ef
  Vendor-rfc1048 Extensions
    DHCP-Message Option 53, length 1: Discover
    Hostname Option 12, length 9: "fedora-vm"

# OFFER
IP 192.168.122.1.67 > 192.168.122.100.68: BOOTP/DHCP, Reply
  Your-IP 192.168.122.100
  Vendor-rfc1048 Extensions
    DHCP-Message Option 53, length 1: Offer
    Server-ID Option 54, length 4: 192.168.122.1
    Lease-Time Option 51, length 4: 3600
    Subnet-Mask Option 1, length 4: 255.255.255.0
    Default-Gateway Option 3, length 4: 192.168.122.1
    Domain-Name-Server Option 6, length 4: 192.168.122.1

# REQUEST
IP 0.0.0.0.68 > 255.255.255.255.67: BOOTP/DHCP, Request
  Client-Ethernet-Address 52:54:00:ab:cd:ef
  Vendor-rfc1048 Extensions
    DHCP-Message Option 53, length 1: Request
    Requested-IP Option 50, length 4: 192.168.122.100
    Server-ID Option 54, length 4: 192.168.122.1

# ACK
IP 192.168.122.1.67 > 192.168.122.100.68: BOOTP/DHCP, Reply
  Your-IP 192.168.122.100
  Vendor-rfc1048 Extensions
    DHCP-Message Option 53, length 1: ACK
    Lease-Time Option 51, length 4: 3600
```

### Qué observar

1. **DISCOVER** y **REQUEST** usan broadcast (0.0.0.0 → 255.255.255.255) — el cliente aún no tiene IP.
2. **OFFER** y **ACK** van dirigidos a la IP propuesta — pero el switch/bridge los entrega al cliente por su MAC.
3. El `Client-Ethernet-Address` (MAC) es lo que identifica al cliente en todo el proceso.
4. El lease time está en las opciones del OFFER y del ACK.

---

## 12. DHCP y NetworkManager/systemd-networkd

Dentro de las VMs (y en el host), el cliente DHCP es gestionado por el sistema de red. Los dos principales en Linux moderno:

### NetworkManager (la mayoría de distros desktop y server)

```bash
# Ver si la interfaz usa DHCP
nmcli device show eth0 | grep "method"
# IP4.METHOD:  auto   ← auto = DHCP

# Ver detalles DHCP recibidos
nmcli device show eth0 | grep DHCP4

# Cambiar a IP estática
nmcli connection modify "Wired connection 1" ipv4.method manual ...

# Cambiar a DHCP
nmcli connection modify "Wired connection 1" ipv4.method auto
nmcli connection up "Wired connection 1"
```

### systemd-networkd (servidores minimalistas, contenedores)

```bash
# Configuración DHCP
cat /etc/systemd/network/10-dhcp.network
# [Match]
# Name=eth0
# [Network]
# DHCP=yes

# Ver estado
networkctl status eth0
# Address: 192.168.122.100 (DHCP4 via 192.168.122.1)
# Gateway: 192.168.122.1

# Forzar renovación
sudo networkctl renew eth0
```

### dhclient (directo, sin NetworkManager)

En instalaciones mínimas que no tienen NetworkManager ni systemd-networkd configurado:

```bash
# Solicitar IP (proceso DORA completo)
sudo dhclient eth0

# Liberar IP
sudo dhclient -r eth0

# Solicitar con output detallado (debug)
sudo dhclient -v eth0
# DHCPDISCOVER on eth0 to 255.255.255.255 port 67
# DHCPOFFER of 192.168.122.100 from 192.168.122.1
# DHCPREQUEST of 192.168.122.100 on eth0
# DHCPACK of 192.168.122.100 from 192.168.122.1
# bound to 192.168.122.100 -- renewal in 1800 seconds
```

---

## 13. Errores comunes

### Error 1: La VM no obtiene IP — "No DHCPOFFERS received"

```bash
# Dentro de la VM:
sudo dhclient -v eth0
# DHCPDISCOVER on eth0 to 255.255.255.255 port 67 interval 3
# DHCPDISCOVER on eth0 to 255.255.255.255 port 67 interval 8
# DHCPDISCOVER on eth0 to 255.255.255.255 port 67 interval 15
# No DHCPOFFERS received.
# No working leases in persistent database - sleeping.
```

**Diagnóstico en el host:**
```bash
# ¿La red virtual está activa?
virsh net-list --all
# Si "inactive" → virsh net-start default

# ¿dnsmasq está corriendo?
ps aux | grep dnsmasq
# Si no aparece → reiniciar libvirtd: sudo systemctl restart libvirtd

# ¿La VM está conectada a la red correcta?
virsh domiflist mi-vm
# Source debe ser "default" (o la red que esperas)

# ¿El bridge virbr0 existe y tiene la IP?
ip addr show virbr0
```

### Error 2: IP duplicada — dos VMs con la misma IP

```bash
# Dentro de la VM:
ip addr show eth0
# inet 192.168.122.100/24

# Pero ping falla intermitentemente o la conexión se pierde

# El problema: dos VMs tienen la misma reserva DHCP (misma IP, diferentes MACs)
# O una VM tiene IP estática que conflicta con el pool DHCP

# Verificar desde el host:
virsh net-dumpxml default | grep "host mac"
# Revisar que no haya dos <host> con la misma ip=""
```

### Error 3: La VM mantiene la IP vieja después de cambiar la red

```bash
# Cambiaste el rango DHCP de la red virtual
# Pero la VM sigue con la IP del rango viejo

# El lease anterior sigue activo en la VM
# Solución: renovar el lease
# Dentro de la VM:
sudo dhclient -r eth0 && sudo dhclient eth0

# O si la VM tiene NetworkManager:
nmcli connection down "Wired connection 1" && \
nmcli connection up "Wired connection 1"
```

### Error 4: Confundir net-destroy con perder leases

```bash
# virsh net-destroy SOLO detiene la red, no borra la configuración
virsh net-destroy default    # detiene la red
virsh net-start default      # la reinicia con la misma configuración

# Los leases se pierden al destruir la red, pero las reservas (<host>)
# están en el XML de la configuración y persisten.

# Para borrar la red permanentemente:
# virsh net-undefine default  ← esto SÍ elimina la configuración
```

### Error 5: Asumir que la IP de la VM es siempre la misma

```bash
# Sin reserva DHCP, la IP puede cambiar:
# - VM se apaga → lease expira → otra VM toma esa IP → VM original recibe otra IP
# - Se reinicia la red virtual → pool se resetea

# Solución: usar reservas DHCP para VMs que necesitan IP estable
virsh net-update default add ip-dhcp-host \
    '<host mac="52:54:00:ab:cd:ef" ip="192.168.122.100"/>' \
    --live --config
```

---

## 14. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                        DHCP CHEATSHEET                              ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  PROCESO DORA                                                      ║
║  1. DISCOVER  Cliente → broadcast "¿hay servidor DHCP?"            ║
║  2. OFFER     Servidor → "te ofrezco IP X, lease Y"                ║
║  3. REQUEST   Cliente → broadcast "acepto IP X"                    ║
║  4. ACK       Servidor → "confirmado, lease de Y segundos"         ║
║  Puertos: servidor=67/UDP, cliente=68/UDP                          ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  QUÉ ENTREGA                                                      ║
║  IP + máscara + gateway + DNS + lease time + hostname              ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  LEASE (CICLO DE VIDA)                                             ║
║     0%      IP activa, funcionando                                 ║
║    50% (T1) Cliente intenta renovar (unicast al servidor)          ║
║  87.5% (T2) Rebind: broadcast a cualquier servidor                 ║
║   100%      IP expira, se pierde                                   ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  LIBVIRT / DNSMASQ                                                 ║
║  virsh net-dhcp-leases default       Ver leases activos            ║
║  virsh net-dumpxml default           Ver config (incluye reservas) ║
║  virsh net-update ... add ip-dhcp-host  Agregar reserva en caliente║
║  virsh net-edit default              Editar XML de la red          ║
║  /var/lib/libvirt/dnsmasq/*.leases   Archivo de leases             ║
║  ps aux | grep dnsmasq              Proceso DHCP/DNS               ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  RESERVA DHCP (IP FIJA POR MAC)                                   ║
║  virsh domiflist <vm>                Obtener MAC                   ║
║  virsh net-update default add ip-dhcp-host \                       ║
║    '<host mac="XX" ip="192.168.122.100"/>' --live --config         ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  DENTRO DE LA VM                                                   ║
║  dhclient -v eth0          Solicitar IP (verbose)                  ║
║  dhclient -r eth0          Liberar IP actual                       ║
║  nmcli device show eth0    Ver config DHCP recibida                ║
║  networkctl renew eth0     Renovar (systemd-networkd)              ║
║  cat /etc/resolv.conf      Ver DNS asignado                       ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  TROUBLESHOOTING                                                   ║
║  No DHCPOFFERS → ¿red activa? ¿dnsmasq corriendo? ¿VM conectada? ║
║  IP duplicada → revisar reservas, verificar pool vs estáticas      ║
║  IP vieja → dhclient -r && dhclient (renovar lease)                ║
║  Capturar DORA: tcpdump -i virbr0 -n -vv port 67 or port 68      ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 15. Ejercicios

### Ejercicio 1: Inspección de DHCP en tu entorno

Ejecuta los siguientes comandos y responde:

```bash
# En el host:
virsh net-dhcp-leases default
virsh net-dumpxml default | grep -A5 dhcp
ps aux | grep dnsmasq
cat /var/lib/libvirt/dnsmasq/default.leases
```

**Preguntas:**
1. ¿Cuántos leases activos tienes? Si no tienes VMs corriendo, ¿esperarías ver leases? ¿Por qué podrían aparecer leases de VMs que ya no corren?
2. ¿El rango DHCP coincide con lo que ves en `virsh net-dumpxml`? ¿Hay reservas `<host>` definidas?
3. ¿Cuántos procesos dnsmasq están corriendo? ¿Uno por cada red virtual activa?
4. En el archivo de leases, ¿el timestamp de expiración ya pasó o aún es futuro? Convierte uno con `date -d @<timestamp>`.

### Ejercicio 2: Ciclo DORA observado

Si tienes una VM corriendo (o puedes arrancar una), captura el proceso DORA:

```bash
# Terminal 1 (host): capturar tráfico DHCP
sudo tcpdump -i virbr0 -n -vv port 67 or port 68

# Terminal 2 (dentro de la VM): forzar renovación
sudo dhclient -r eth0 && sleep 2 && sudo dhclient -v eth0
```

**Preguntas:**
1. ¿Viste los 4 mensajes (Discover, Offer, Request, ACK)? Si solo viste Request y ACK, ¿por qué? (pista: el cliente ya tenía un lease anterior).
2. ¿De qué IP a qué IP va el Discover? ¿Y el Offer?
3. ¿Qué opciones DHCP aparecen en el ACK? ¿Ves lease-time, router, DNS?
4. ¿Cuánto es el lease-time en segundos? ¿A qué hora expiraría?

### Ejercicio 3: Reservas DHCP para un lab

Tienes tres VMs que necesitan IPs estables para un lab:

| VM | MAC (ficticia) | Servicio | IP deseada |
|----|----------------|---------|------------|
| web | 52:54:00:aa:bb:01 | nginx | 192.168.122.10 |
| api | 52:54:00:aa:bb:02 | backend | 192.168.122.11 |
| db | 52:54:00:aa:bb:03 | postgres | 192.168.122.12 |

1. Escribe los tres comandos `virsh net-update` para crear las reservas.
2. ¿Qué sucedería si la VM "web" ya tiene la IP .100 por DHCP dinámico? ¿Obtendría automáticamente la .10 o necesitas hacer algo?
3. Si destruyes y recreas la red (`virsh net-destroy default && virsh net-start default`), ¿las reservas sobreviven? ¿Y los leases dinámicos?
4. Escribe el fragmento XML de `<dhcp>` que incluya las tres reservas y un rango dinámico de .50 a .254 (dejando .2-.49 sin usar y .10-.12 reservadas).

**Pregunta reflexiva:** ¿Por qué es mejor usar reservas DHCP que configurar IP estática dentro de cada VM para un lab? Piensa en qué pasa cuando necesitas cambiar el gateway o el DNS de todas las VMs a la vez.

---

> **Nota**: DHCP es el protocolo que convierte la tarea manual de "configurar red en cada VM" en algo automático y centralizado. En libvirt, dnsmasq se encarga de todo: asigna IPs, entrega gateway y DNS, y registra hostnames para resolución. Las reservas DHCP son la herramienta clave para labs con VMs que necesitan IPs predecibles sin perder la ventaja de la configuración centralizada. Antes de configurar IP estática manualmente dentro de una VM, pregúntate si una reserva DHCP no sería más limpia.
