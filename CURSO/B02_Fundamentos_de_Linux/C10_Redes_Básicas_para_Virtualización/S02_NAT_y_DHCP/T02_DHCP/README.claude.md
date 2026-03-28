# T02 — DHCP (Dynamic Host Configuration Protocol)

## Erratas detectadas en los archivos fuente

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| `README.max.md` | 168 | `sudo dhclient -r && sleep 1 && dhclient` — falta `sudo` en el segundo `dhclient`. Sin privilegios de root, `dhclient` no puede vincular el puerto 68 ni configurar la interfaz. | `sudo dhclient -r && sleep 1 && sudo dhclient` |
| `README.md` | 329 | `date -d @1710864215` muestra `Tue Mar 19 16:30:15 CET 2026`. El epoch 1710864215 corresponde a marzo de **2024**, no 2026. Un estudiante que ejecute el comando obtendrá un año distinto al documentado. | Salida correcta: `Tue Mar 19 ...  2024` |

---

## 1. El problema: asignar IPs a mano no escala

Cada dispositivo en una red necesita al menos cuatro parametros configurados:

```
IP:       192.168.122.100
Mascara:  255.255.255.0
Gateway:  192.168.122.1
DNS:      192.168.122.1
```

Configurar esto manualmente en 3 VMs es tolerable. En 30 VMs es tedioso. En 300 es inviable. Y hay problemas adicionales:

- **Conflictos de IP**: si asignas la misma IP a dos dispositivos, ambos pierden conectividad.
- **Cambios de configuracion**: si el gateway cambia de .1 a .254, debes actualizar cada dispositivo.
- **Rotacion de dispositivos**: las VMs se crean y destruyen constantemente — gestionar manualmente un pool de IPs es propenso a errores.

DHCP resuelve todos estos problemas: un servidor central asigna IPs automaticamente, evita duplicados, y propaga cambios de configuracion.

---

## 2. Que es DHCP

DHCP (Dynamic Host Configuration Protocol) es un protocolo cliente-servidor que asigna automaticamente configuracion de red a los dispositivos que se conectan. Opera usando UDP:

| Componente | Puerto UDP | Rol |
|-----------|-----------|-----|
| Servidor DHCP | 67 | Escucha peticiones, asigna IPs |
| Cliente DHCP | 68 | Solicita configuracion de red |

### Por que UDP y no TCP

TCP requiere una conexion establecida, que a su vez requiere una IP. Pero el cliente DHCP **no tiene IP** — precisamente por eso esta pidiendo una. UDP no necesita conexion previa: el cliente envia un datagrama broadcast desde 0.0.0.0 (sin IP origen) y el servidor responde.

### Participantes en un entorno de virtualizacion

```
+-- HOST ----------------------------------------------+
|                                                       |
|  dnsmasq (servidor DHCP de libvirt)                   |
|  Escucha en virbr0 (192.168.122.1)                   |
|  Pool: 192.168.122.2 - 192.168.122.254               |
|                                                       |
|  +--- VM1 ----------+    +--- VM2 ----------+        |
|  | Cliente DHCP      |    | Cliente DHCP      |        |
|  | (dhclient o       |    | (dhclient o       |        |
|  |  systemd-         |    |  systemd-         |        |
|  |  networkd)        |    |  networkd)        |        |
|  |                   |    |                   |        |
|  | Obtiene:          |    | Obtiene:          |        |
|  | .122.100          |    | .122.101          |        |
|  +-------------------+    +-------------------+        |
+-------------------------------------------------------+
```

---

## 3. El proceso DORA paso a paso

DHCP usa un intercambio de 4 mensajes conocido como **DORA**: Discover, Offer, Request, Acknowledge.

### Diagrama temporal

```
    Cliente (sin IP)                     Servidor DHCP (192.168.122.1)
         |                                        |
    +----+  1. DHCP DISCOVER                      |
    |    |---- broadcast --------------------------->
    |    |  src=0.0.0.0:68  dst=255.255.255.255:67|
    |    |  "Hay algun servidor DHCP?"             |
    |    |                                        |
    |    |  2. DHCP OFFER                          |
    |    | <-----------------------------------------|
    |    |  "Te ofrezco 192.168.122.100,           |
    |    |   gateway .1, DNS .1, lease 3600s"      |
    |    |                                        |
    |    |  3. DHCP REQUEST                        |
    |    |---- broadcast --------------------------->
    |    |  "Acepto 192.168.122.100"               |
    |    |  (broadcast porque otros servidores     |
    |    |   DHCP deben saber que rechace          |
    |    |   sus ofertas)                           |
    |    |                                        |
    |    |  4. DHCP ACK                            |
    |    | <-----------------------------------------|
    |    |  "Confirmado. Lease de 3600s.            |
    +----+   Tu IP es 192.168.122.100."            |
         |                                        |
    Cliente configura su interfaz:                |
    ip=192.168.122.100/24                         |
    gw=192.168.122.1                              |
    dns=192.168.122.1                             |
```

### Detalles de cada paso

**1. DISCOVER** — el cliente grita al vacio

El cliente no sabe si hay un servidor DHCP ni donde esta. Envia un broadcast Ethernet (dst MAC: ff:ff:ff:ff:ff:ff) y un broadcast IP (dst: 255.255.255.255). Como no tiene IP propia, usa 0.0.0.0 como origen.

El mensaje incluye la MAC del cliente — el servidor la necesitara para responder y para posibles reservas.

**2. OFFER** — el servidor propone

El servidor selecciona una IP libre de su pool y se la ofrece al cliente. La oferta incluye:
- IP propuesta
- Mascara de red
- Gateway
- DNS
- Duracion del lease

Si hay multiples servidores DHCP, cada uno envia su propio OFFER. El cliente elige uno (normalmente el primero que llega).

**3. REQUEST** — el cliente acepta

El cliente anuncia (en broadcast) que oferta acepta. Hace broadcast por dos razones:
- El servidor elegido confirma la asignacion.
- Los demas servidores (si los hay) retiran sus ofertas y liberan las IPs propuestas.

**4. ACK** — el servidor confirma

El servidor confirma la asignacion. El cliente configura su interfaz de red y empieza a usar la IP.

Si el servidor detecta que la IP ya esta en uso (hace un ARP para verificar), envia un **NACK** y el proceso reinicia.

### Y si no hay servidor DHCP

Si el cliente no recibe un OFFER despues de varios intentos, se auto-asigna una IP link-local (169.254.x.x) mediante APIPA. Como vimos en T03 de S01, esta IP no es funcional para comunicacion general.

---

## 4. Que entrega el servidor DHCP

DHCP no solo asigna una IP — entrega un paquete completo de configuracion de red a traves de **opciones DHCP** (DHCP options):

### Opciones estandar

| Opcion | Codigo | Que configura | Ejemplo |
|--------|--------|---------------|---------|
| Subnet Mask | 1 | Mascara de red | 255.255.255.0 |
| Router | 3 | Gateway por defecto | 192.168.122.1 |
| DNS Servers | 6 | Servidores DNS | 192.168.122.1, 8.8.8.8 |
| Domain Name | 15 | Dominio de busqueda | lab.local |
| Lease Time | 51 | Duracion del alquiler | 3600 (1 hora) |
| DHCP Server ID | 54 | IP del servidor DHCP | 192.168.122.1 |
| NTP Servers | 42 | Servidores de hora | 192.168.122.1 |
| Hostname | 12 | Nombre del host | fedora-vm |

### En libvirt

La red default de libvirt entrega:
- **IP**: del pool 192.168.122.2-254
- **Mascara**: 255.255.255.0 (/24)
- **Gateway**: 192.168.122.1
- **DNS**: 192.168.122.1 (dnsmasq del host actua como DNS forwarder)

```bash
# Ver que DNS entrego DHCP (dentro de la VM):
cat /etc/resolv.conf
# nameserver 192.168.122.1

# Verificar que el DNS funciona:
dig google.com @192.168.122.1
```

El dnsmasq de libvirt reenvia las consultas DNS al DNS configurado en el host — no resuelve por si mismo. Si el host usa 8.8.8.8 como DNS, las VMs tambien lo usaran indirectamente.

---

## 5. El lease: alquiler temporal de la IP

Las IPs asignadas por DHCP no son permanentes — se "alquilan" por un periodo definido por el servidor. Esto permite reutilizar IPs de dispositivos que ya no estan conectados.

### Anatomia de un lease

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
| `dhcp-lease-time` | Duracion total del lease en segundos |
| `renew` | Momento en que el cliente intentara renovar (50% del lease) |
| `rebind` | Si renew falla, momento del segundo intento (87.5% del lease) |
| `expire` | Momento en que la IP expira y se libera |

### Tiempos de lease tipicos

| Contexto | Lease time | Razon |
|----------|-----------|-------|
| Red domestica | 24 horas | Los dispositivos suelen estar conectados mucho tiempo |
| Red de oficina | 8 horas | Jornada laboral |
| Cafeteria/WiFi publico | 1-2 horas | Alta rotacion de dispositivos |
| libvirt default | 1 hora | Las VMs pueden crearse/destruirse frecuentemente |
| Red de lab | 1-2 horas | Entorno dinamico |

### Que pasa si una VM se apaga sin liberar el lease

El lease sigue activo en el servidor. La IP queda "reservada" hasta que el lease expire. Si la VM vuelve a arrancar antes de que expire, pedira la misma IP (DHCP REQUEST con la IP anterior) y normalmente la obtendra.

Si otra VM se crea mientras la primera esta apagada y el lease aun no expiro, la nueva VM recibira una IP diferente — el servidor no reasigna una IP con lease activo.

---

## 6. Renovacion y expiracion del lease

### Linea temporal de un lease

```
Tiempo 0          50%            87.5%         100%
   |               |               |              |
   +---------------+---------------+--------------+
   |               |               |              |
   |  IP activa    |  RENEW        |  REBIND      |  EXPIRE
   |  funcionando  |  unicast al   |  broadcast   |  IP se
   |  normalmente  |  servidor     |  a cualquier |  pierde
   |               |  original     |  servidor    |
```

**50% (T1 — Renew)**: el cliente envia un REQUEST **unicast** al servidor que le dio el lease. Si el servidor responde ACK, el lease se extiende otro periodo completo.

**87.5% (T2 — Rebind)**: si el renew fallo (servidor caido, red cambiada), el cliente envia un REQUEST **broadcast** a cualquier servidor DHCP en la red. Si otro servidor responde, le asigna una nueva IP.

**100% (Expire)**: si nadie respondio, el cliente pierde la IP. Debe reiniciar el proceso DORA desde cero. La interfaz de red queda sin IP funcional.

### Renovar manualmente

```bash
# Dentro de una VM o en el host:

# Liberar la IP actual (envia DHCP Release al servidor)
sudo dhclient -r eth0

# Solicitar una nueva IP (proceso DORA completo)
sudo dhclient eth0

# Ver el estado DHCP con NetworkManager:
nmcli connection show "Wired connection 1" | grep DHCP

# Forzar reconexion con NetworkManager:
nmcli connection down "Wired connection 1" && \
nmcli connection up "Wired connection 1"
```

### Cambia la IP al renovar?

Normalmente **no**. El cliente solicita la misma IP que tenia, y el servidor la confirma si sigue disponible. La IP cambia solo si:
- El lease expiro completamente.
- El servidor fue reconfigurado (rango diferente).
- Otro dispositivo tomo esa IP mientras el cliente estuvo desconectado.

---

## 7. DHCP en libvirt: dnsmasq

Libvirt no implementa DHCP desde cero — usa **dnsmasq**, un servidor ligero que combina DHCP y DNS forwarding en un solo proceso.

### dnsmasq: el servidor detras de libvirt

Cuando activas una red virtual con libvirt, se lanza un proceso dnsmasq dedicado a esa red:

```bash
# Ver el proceso dnsmasq de la red default
ps aux | grep dnsmasq
# nobody  1234  dnsmasq --conf-file=/var/lib/libvirt/dnsmasq/default.conf
#                       --dhcp-range=192.168.122.2,192.168.122.254,255.255.255.0
#                       --listen-address=192.168.122.1
#                       --dhcp-leasefile=/var/lib/libvirt/dnsmasq/default.leases
```

### Archivos de configuracion y leases

| Archivo | Contenido |
|---------|-----------|
| `/var/lib/libvirt/dnsmasq/default.conf` | Configuracion generada por libvirt (no editar manualmente) |
| `/var/lib/libvirt/dnsmasq/default.leases` | Leases activos (formato texto) |
| `/var/lib/libvirt/dnsmasq/default.addnhosts` | Entradas /etc/hosts adicionales para la red |

### Formato del archivo de leases

```bash
cat /var/lib/libvirt/dnsmasq/default.leases
# 1710864215 52:54:00:ab:cd:ef 192.168.122.100 fedora-vm 01:52:54:00:ab:cd:ef
# |          |                 |                |          |
# |          |                 |                |          +-- client-id
# |          |                 |                +-- hostname
# |          |                 +-- IP asignada
# |          +-- MAC address
# +-- timestamp de expiracion (epoch)
```

Para convertir el timestamp a fecha legible:

```bash
date -d @1710864215
# Tue Mar 19 16:30:15 CET 2024
```

### Cada red tiene su propio dnsmasq

Si creas multiples redes virtuales, cada una tiene su propio proceso dnsmasq, su propio archivo de leases, y su propio pool de IPs:

```bash
ps aux | grep dnsmasq | grep -v grep
# dnsmasq --conf-file=.../default.conf --listen-address=192.168.122.1  <- red default
# dnsmasq --conf-file=.../lab-net.conf --listen-address=10.0.1.1       <- red lab-net
```

### DNS integrado en dnsmasq

Dnsmasq no solo asigna IPs — tambien resuelve nombres para las VMs. Si una VM se llama "fedora-vm", dnsmasq registra el nombre y otras VMs en la misma red pueden resolverlo:

```bash
# Desde otra VM en la misma red:
ping fedora-vm
# PING fedora-vm (192.168.122.100) ...

# Esto funciona porque dnsmasq registra hostname -> IP
# cuando asigna el lease DHCP
```

---

## 8. Reservas DHCP: IP fija por MAC

Una reserva DHCP (o "static lease") garantiza que una VM siempre reciba la misma IP, basandose en su direccion MAC.

### Por que reservar

- **Port forwarding**: si configuras DNAT hacia 192.168.122.100 y la VM obtiene .101 por DHCP dinamico, el forwarding deja de funcionar.
- **Scripts de automatizacion**: `ssh user@192.168.122.100` deja de funcionar si la IP cambia.
- **Referencia estable**: saber siempre en que IP esta cada VM.

### Metodo 1: virsh net-update (en caliente)

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
# --config: guardar en la configuracion persistente
```

### Metodo 2: virsh net-edit (edicion del XML)

```bash
virsh net-edit default
```

Dentro de la seccion `<dhcp>`, anadir:

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

Despues de editar:
```bash
# Reiniciar la red para aplicar cambios (si no usaste --live)
virsh net-destroy default && virsh net-start default

# Las VMs necesitaran renovar su lease:
# Dentro de cada VM: sudo dhclient -r eth0 && sudo dhclient eth0
# O simplemente reiniciar la VM
```

### Eliminar una reserva

```bash
virsh net-update default delete ip-dhcp-host \
    '<host mac="52:54:00:ab:cd:ef" name="fedora-vm" ip="192.168.122.100"/>' \
    --live --config
```

### Reserva vs IP estatica manual

| Caracteristica | Reserva DHCP | IP estatica |
|---------------|-------------|------------|
| Configuracion | En el servidor (virsh net-update) | En cada VM (/etc/network/interfaces) |
| Gateway/DNS | Entregados automaticamente | Configurados manualmente |
| Cambios de red | Se propagan automaticamente | Hay que actualizar cada VM |
| IP garantizada | Si (por MAC) | Si (configurada a mano) |
| Funciona sin DHCP server | No | Si |

La reserva DHCP combina lo mejor de ambos mundos: IP predecible + configuracion centralizada.

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

# Ver configuracion completa de la red (incluye reservas)
virsh net-dumpxml default
```

### Desde la VM

```bash
# Ver la configuracion DHCP recibida (NetworkManager)
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

# Ver configuracion de red resultante
ip addr show eth0
ip route show default
cat /etc/resolv.conf
```

### Asociar una IP a una VM sin virsh net-dhcp-leases

Si `virsh net-dhcp-leases` no muestra la VM (puede pasar si el lease expiro o si la VM tiene IP estatica), alternativas:

```bash
# ARP: ver que MACs estan activas en la red
arp -n | grep 192.168.122

# nmap: escanear la red (requiere nmap)
sudo nmap -sn 192.168.122.0/24

# Consola de la VM
virsh console fedora-vm
# (dentro de la VM) ip addr show
```

---

## 10. IP estatica vs DHCP: cuando usar cada una

### En el host

| Situacion | Recomendacion |
|-----------|--------------|
| Desktop/laptop domestico | DHCP (el router asigna) |
| Servidor de produccion | IP estatica (debe ser predecible) |
| Host de virtualizacion | IP estatica en LAN, DHCP en virbr0 no aplica (es el servidor) |

### En las VMs

| Situacion | Recomendacion |
|-----------|--------------|
| VM de desarrollo general | DHCP (default de libvirt) |
| VM que ofrece servicios (web, DB) | Reserva DHCP (predecible + centralizada) |
| VM en lab temporal (se destruye pronto) | DHCP (no importa la IP) |
| VM aislada sin red libvirt | IP estatica (no hay servidor DHCP) |

### Configurar IP estatica dentro de una VM

Si necesitas IP estatica sin depender de DHCP:

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
# Con /etc/network/interfaces (Debian/Ubuntu server clasico)
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

Observar el proceso DORA con tcpdump es una excelente forma de entender DHCP en la practica.

### Captura desde el host

```bash
# Terminal 1: capturar trafico DHCP en el bridge de libvirt
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

### Que observar

1. **DISCOVER** y **REQUEST** usan broadcast (0.0.0.0 -> 255.255.255.255) — el cliente aun no tiene IP.
2. **OFFER** y **ACK** van dirigidos a la IP propuesta — pero el switch/bridge los entrega al cliente por su MAC.
3. El `Client-Ethernet-Address` (MAC) es lo que identifica al cliente en todo el proceso.
4. El lease time esta en las opciones del OFFER y del ACK.

---

## 12. DHCP y NetworkManager/systemd-networkd

Dentro de las VMs (y en el host), el cliente DHCP es gestionado por el sistema de red. Los dos principales en Linux moderno:

### NetworkManager (la mayoria de distros desktop y server)

```bash
# Ver si la interfaz usa DHCP
nmcli device show eth0 | grep "method"
# IP4.METHOD:  auto   <- auto = DHCP

# Ver detalles DHCP recibidos
nmcli device show eth0 | grep DHCP4

# Cambiar a IP estatica
nmcli connection modify "Wired connection 1" ipv4.method manual ...

# Cambiar a DHCP
nmcli connection modify "Wired connection 1" ipv4.method auto
nmcli connection up "Wired connection 1"
```

### systemd-networkd (servidores minimalistas, contenedores)

```bash
# Configuracion DHCP
cat /etc/systemd/network/10-dhcp.network
# [Match]
# Name=eth0
# [Network]
# DHCP=yes

# Ver estado
networkctl status eth0
# Address: 192.168.122.100 (DHCP4 via 192.168.122.1)
# Gateway: 192.168.122.1

# Forzar renovacion
sudo networkctl renew eth0
```

### dhclient (directo, sin NetworkManager)

En instalaciones minimas que no tienen NetworkManager ni systemd-networkd configurado:

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

**Diagnostico en el host:**
```bash
# 1. La red virtual esta activa?
virsh net-list --all
# Si "inactive" -> virsh net-start default

# 2. dnsmasq esta corriendo?
ps aux | grep dnsmasq
# Si no aparece -> reiniciar libvirtd: sudo systemctl restart libvirtd

# 3. La VM esta conectada a la red correcta?
virsh domiflist mi-vm
# Source debe ser "default" (o la red que esperas)

# 4. El bridge virbr0 existe y tiene la IP?
ip addr show virbr0
```

### Error 2: IP duplicada — dos VMs con la misma IP

```bash
# Dentro de la VM: ping falla intermitentemente o la conexion se pierde

# El problema: dos VMs tienen la misma reserva DHCP (misma IP, diferentes MACs)
# O una VM tiene IP estatica que conflicta con el pool DHCP

# Verificar desde el host:
virsh net-dumpxml default | grep "host mac"
# Revisar que no haya dos <host> con la misma ip=""
```

### Error 3: La VM mantiene la IP vieja despues de cambiar la red

```bash
# Cambiaste el rango DHCP de la red virtual
# Pero la VM sigue con la IP del rango viejo

# El lease anterior sigue activo en la VM
# Solucion: renovar el lease
# Dentro de la VM:
sudo dhclient -r eth0 && sudo dhclient eth0

# O si la VM tiene NetworkManager:
nmcli connection down "Wired connection 1" && \
nmcli connection up "Wired connection 1"
```

### Error 4: Confundir net-destroy con perder leases

```bash
# virsh net-destroy SOLO detiene la red, no borra la configuracion
virsh net-destroy default    # detiene la red
virsh net-start default      # la reinicia con la misma configuracion

# Los leases se pierden al destruir la red, pero las reservas (<host>)
# estan en el XML de la configuracion y persisten.

# Para borrar la red permanentemente:
# virsh net-undefine default  <- esto SI elimina la configuracion
```

### Error 5: Asumir que la IP de la VM es siempre la misma

```bash
# Sin reserva DHCP, la IP puede cambiar:
# - VM se apaga -> lease expira -> otra VM toma esa IP -> VM original recibe otra IP
# - Se reinicia la red virtual -> pool se resetea

# Solucion: usar reservas DHCP para VMs que necesitan IP estable
virsh net-update default add ip-dhcp-host \
    '<host mac="52:54:00:ab:cd:ef" ip="192.168.122.100"/>' \
    --live --config
```

---

## 14. Cheatsheet

```text
+======================================================================+
|                        DHCP CHEATSHEET                                |
+======================================================================+
|                                                                      |
|  PROCESO DORA                                                        |
|  1. DISCOVER  Cliente -> broadcast "hay servidor DHCP?"              |
|  2. OFFER     Servidor -> "te ofrezco IP X, lease Y"                 |
|  3. REQUEST   Cliente -> broadcast "acepto IP X"                     |
|  4. ACK       Servidor -> "confirmado, lease de Y segundos"          |
|  Puertos: servidor=67/UDP, cliente=68/UDP                            |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  QUE ENTREGA                                                         |
|  IP + mascara + gateway + DNS + lease time + hostname                |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  LEASE (CICLO DE VIDA)                                               |
|     0%      IP activa, funcionando                                   |
|    50% (T1) Cliente intenta renovar (unicast al servidor)            |
|  87.5% (T2) Rebind: broadcast a cualquier servidor                   |
|   100%      IP expira, se pierde                                     |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  LIBVIRT / DNSMASQ                                                   |
|  virsh net-dhcp-leases default       Ver leases activos              |
|  virsh net-dumpxml default           Ver config (incluye reservas)   |
|  virsh net-update ... add ip-dhcp-host  Agregar reserva en caliente  |
|  virsh net-edit default              Editar XML de la red            |
|  /var/lib/libvirt/dnsmasq/*.leases   Archivo de leases               |
|  ps aux | grep dnsmasq              Proceso DHCP/DNS                 |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  RESERVA DHCP (IP FIJA POR MAC)                                     |
|  virsh domiflist <vm>                Obtener MAC                     |
|  virsh net-update default add ip-dhcp-host \                         |
|    '<host mac="XX" ip="192.168.122.100"/>' --live --config           |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  DENTRO DE LA VM                                                     |
|  dhclient -v eth0          Solicitar IP (verbose)                    |
|  dhclient -r eth0          Liberar IP actual                         |
|  nmcli device show eth0    Ver config DHCP recibida                  |
|  networkctl renew eth0     Renovar (systemd-networkd)                |
|  cat /etc/resolv.conf      Ver DNS asignado                          |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  TROUBLESHOOTING                                                     |
|  No DHCPOFFERS -> red activa? dnsmasq corriendo? VM conectada?       |
|  IP duplicada -> revisar reservas, verificar pool vs estaticas       |
|  IP vieja -> dhclient -r && dhclient (renovar lease)                 |
|  Capturar DORA: tcpdump -i virbr0 -n -vv port 67 or port 68        |
|                                                                      |
+======================================================================+
```

---

## 15. Ejercicios

### Ejercicio 1: Puertos DHCP y razon de UDP

DHCP usa puertos 67 (servidor) y 68 (cliente) sobre UDP.

**Pregunta:** Si un cliente DHCP necesita TCP para funcionar, que problema fundamental impediria que el proceso DORA se completara?

<details><summary>Prediccion</summary>

TCP requiere un three-way handshake (SYN, SYN-ACK, ACK) que necesita una IP de origen valida. Pero el cliente DHCP aun no tiene IP — esta pidiendo una. No puede establecer una conexion TCP desde 0.0.0.0 porque TCP necesita que ambos extremos tengan direcciones IP para gestionar la conexion. UDP no tiene este problema: el cliente simplemente envia un datagrama broadcast desde 0.0.0.0:68 a 255.255.255.255:67 sin conexion previa.

</details>

---

### Ejercicio 2: Orden del proceso DORA

Un cliente se conecta a una red con dos servidores DHCP (el router domestico y un dnsmasq de libvirt).

**Pregunta:** En el paso 3 (REQUEST), por que el cliente envia broadcast en vez de unicast directamente al servidor que eligio?

<details><summary>Prediccion</summary>

El REQUEST se envia en broadcast para que **todos** los servidores DHCP en la red lo reciban. El servidor elegido confirma la asignacion con un ACK. Los demas servidores (en este caso, el segundo servidor cuya OFFER fue rechazada) ven el REQUEST, notan que el client eligio otro servidor, y liberan la IP que habian reservado para esta oferta. Si el REQUEST fuera unicast, los servidores rechazados mantendrian la IP reservada indefinidamente sin saber que el cliente eligio a otro.

</details>

---

### Ejercicio 3: Calcular momentos del lease

Una VM recibe un lease de 3600 segundos (1 hora) a las 14:00:00.

**Pregunta:** Calcula a que hora exacta ocurren T1 (renew), T2 (rebind) y la expiracion. Si a las 14:31 el servidor DHCP se cae, que pasara?

<details><summary>Prediccion</summary>

- **T1 (Renew, 50%)**: 3600 * 0.50 = 1800s despues = **14:30:00**. El cliente envia REQUEST unicast al servidor original.
- **T2 (Rebind, 87.5%)**: 3600 * 0.875 = 3150s despues = **14:52:30**. El cliente envia REQUEST broadcast a cualquier servidor.
- **Expiracion (100%)**: 3600s despues = **15:00:00**. La IP se pierde.

Si el servidor se cae a las 14:31: el T1 (14:30) ya paso y el renew unicast fallo (servidor caido). El cliente esperara hasta T2 (14:52:30) para hacer rebind broadcast. Si no hay otro servidor DHCP, a las 15:00:00 la IP expira y la VM pierde conectividad. En una red libvirt tipica con un solo dnsmasq, esto significa que la VM quedara sin IP hasta que dnsmasq se reinicie.

</details>

---

### Ejercicio 4: VM apagada y lease

Tienes una VM "web-server" con IP 192.168.122.100 obtenida por DHCP con lease de 1 hora. Apagas la VM sin liberar el lease. 30 minutos despues, creas una nueva VM "test-vm".

**Pregunta:** Que IP obtendra "test-vm"? Si re-arrancas "web-server" 45 minutos despues de apagarla, que IP obtendra?

<details><summary>Prediccion</summary>

- **test-vm** (30 min despues): obtendra una IP **diferente** a .100 (por ejemplo, .101). El lease de web-server sigue activo en el servidor (aun no han pasado los 60 min), asi que dnsmasq no reasigna .100.
- **web-server** (45 min despues, lease aun no expirado): al arrancar, el cliente DHCP enviara un REQUEST pidiendo su IP anterior (.100). Como el lease sigue vigente (45 < 60 min) y la IP no fue reasignada, el servidor responde ACK y web-server recupera .100.

Si web-server arrancara despues de 70 minutos (lease ya expirado), dependeria de si otra VM tomo .100 en el intermedio. Si .100 esta libre, probablemente la recupere; si no, recibira otra IP del pool.

</details>

---

### Ejercicio 5: Diagnostico — "No DHCPOFFERS received"

Arrancas una VM y ejecutas `sudo dhclient -v eth0` dentro de ella. Obtienes:

```
DHCPDISCOVER on eth0 to 255.255.255.255 port 67 interval 3
DHCPDISCOVER on eth0 to 255.255.255.255 port 67 interval 8
No DHCPOFFERS received.
```

**Pregunta:** Lista los 4 pasos de diagnostico que ejecutarias en el host, en orden, y que buscas en cada uno.

<details><summary>Prediccion</summary>

1. **`virsh net-list --all`** — verificar que la red virtual esta activa (state "active"). Si aparece como "inactive", el dnsmasq no esta corriendo y no hay quien responda al DISCOVER. Solucion: `virsh net-start default`.

2. **`ps aux | grep dnsmasq`** — verificar que el proceso dnsmasq existe. Puede pasar que la red parezca activa pero dnsmasq haya muerto. Si no aparece, reiniciar libvirtd: `sudo systemctl restart libvirtd`.

3. **`virsh domiflist mi-vm`** — verificar que la VM esta conectada a la red correcta. La columna "Source" debe mostrar "default" (o la red que tiene DHCP). Si la VM esta conectada a otra red (o a ninguna), no recibira respuesta.

4. **`ip addr show virbr0`** — verificar que el bridge existe y tiene la IP esperada (192.168.122.1). Si virbr0 no existe o no tiene IP, la red virtual no esta configurada correctamente.

</details>

---

### Ejercicio 6: Reserva DHCP en caliente

Tienes una VM "db-server" con MAC `52:54:00:db:00:01` que actualmente tiene la IP .105 asignada por DHCP dinamico. Necesitas que siempre tenga .50.

**Pregunta:** Escribe el comando para crear la reserva. Despues de ejecutarlo, la VM obtendra automaticamente .50 o necesitas hacer algo mas?

<details><summary>Prediccion</summary>

Comando:
```bash
virsh net-update default add ip-dhcp-host \
    '<host mac="52:54:00:db:00:01" name="db-server" ip="192.168.122.50"/>' \
    --live --config
```

La VM **no** cambiara automaticamente a .50. El lease actual (.105) sigue vigente hasta que expire. Para que la VM obtenga .50 inmediatamente, hay que forzar la renovacion dentro de la VM:

```bash
sudo dhclient -r eth0 && sudo dhclient eth0
```

Esto libera el lease de .105 y hace un nuevo DORA. El servidor ve la MAC, la encuentra en las reservas, y asigna .50.

Alternativa: reiniciar la VM (menos elegante pero funciona).

</details>

---

### Ejercicio 7: Interpretar archivo de leases

El archivo `/var/lib/libvirt/dnsmasq/default.leases` contiene:

```
1711540800 52:54:00:aa:bb:cc 192.168.122.100 web-vm 01:52:54:00:aa:bb:cc
1711537200 52:54:00:dd:ee:ff 192.168.122.101 db-vm 01:52:54:00:dd:ee:ff
```

La hora actual es epoch 1711540000.

**Pregunta:** Que VM tiene el lease aun vigente? Cual ya expiro? Que pasara si intentas hacer ping a la VM con lease expirado?

<details><summary>Prediccion</summary>

- **web-vm** (expira en 1711540800): faltan 800 segundos (~13 minutos). Lease **vigente**. La VM tiene .100 y es accesible.
- **db-vm** (expiro en 1711537200): ya paso hace 2800 segundos (~46 minutos). Lease **expirado**.

Si intentas hacer `ping 192.168.122.101`:
- Si db-vm sigue encendida, puede que responda aun — la VM mantiene la IP configurada en su interfaz aunque el servidor considere el lease expirado. El problema no es inmediato.
- Si db-vm se apago, nadie respondera al ping. El ARP para .101 no obtendra respuesta.
- Si db-vm intenta renovar con el servidor, este puede asignarle una IP diferente (si .101 ya fue reasignada) o confirmar .101 (si esta libre).

Punto clave: la expiracion del lease en el servidor y la configuracion de red en el cliente son independientes. El servidor libera la IP para reasignacion, pero la VM no pierde su IP instantaneamente.

</details>

---

### Ejercicio 8: dnsmasq — DNS integrado

Tienes dos VMs en la red default: "web" (192.168.122.100) y "api" (192.168.122.101). Desde "web" ejecutas `ping api`.

**Pregunta:** Como resuelve dnsmasq el nombre "api" a una IP? Que pasaria si "api" tuviera IP estatica configurada manualmente (sin DHCP)?

<details><summary>Prediccion</summary>

Cuando dnsmasq asigna un lease DHCP, registra el hostname que el cliente envia en el DISCOVER (Option 12: Hostname). Asi dnsmasq sabe que "api" = 192.168.122.101. Cuando "web" hace `ping api`, su resolver consulta al DNS en 192.168.122.1 (el dnsmasq), que responde con .101.

Si "api" tuviera IP estatica **sin DHCP**, dnsmasq **no** tendria el registro hostname -> IP porque nunca hubo un intercambio DHCP donde "api" anunciara su nombre. El `ping api` fallaria con "Name or service not known".

Solucion para IP estatica: anadir manualmente la entrada en `/var/lib/libvirt/dnsmasq/default.addnhosts`:
```
192.168.122.101 api
```
O usar una reserva DHCP (mejor opcion) para que dnsmasq registre el nombre.

</details>

---

### Ejercicio 9: Fragmento XML con reservas

Necesitas configurar la red default de libvirt con:
- Rango dinamico: .50 a .254
- Reservas: web=.10, api=.11, db=.12
- Las IPs .2 a .49 no deben ser asignadas dinamicamente

**Pregunta:** Escribe el fragmento XML completo de `<ip>` con `<dhcp>`. Las IPs reservadas (.10, .11, .12) estan fuera del rango dinamico (.50-.254) — esto es un problema?

<details><summary>Prediccion</summary>

```xml
<ip address='192.168.122.1' netmask='255.255.255.0'>
  <dhcp>
    <range start='192.168.122.50' end='192.168.122.254'/>
    <host mac='52:54:00:aa:bb:01' name='web' ip='192.168.122.10'/>
    <host mac='52:54:00:aa:bb:02' name='api' ip='192.168.122.11'/>
    <host mac='52:54:00:aa:bb:03' name='db' ip='192.168.122.12'/>
  </dhcp>
</ip>
```

Las IPs reservadas (.10-.12) estan **fuera** del rango dinamico (.50-.254). Esto **no es un problema** — de hecho, es una buena practica. Las reservas DHCP (static leases) funcionan por MAC independientemente del rango dinamico. Al mantenerlas fuera del rango, te aseguras de que:
- Ninguna VM nueva recibira accidentalmente .10, .11 o .12 por asignacion dinamica.
- Las IPs reservadas y las dinamicas no pueden conflictar.
- Tienes una separacion clara: .2-.49 para reservas, .50-.254 para dinamicas.

</details>

---

### Ejercicio 10: net-destroy vs net-undefine

Ejecutas los siguientes comandos y tu companero te pregunta si perdio las reservas DHCP:

```bash
virsh net-destroy default
virsh net-start default
```

**Pregunta:** Se pierden las reservas `<host>` del XML? Se pierden los leases dinamicos activos? Que pasaria en cambio si hubiera ejecutado `virsh net-undefine default`?

<details><summary>Prediccion</summary>

**`net-destroy` + `net-start`:**
- **Reservas `<host>`**: **NO se pierden**. Las reservas estan en el XML de la configuracion persistente de la red (almacenado en `/etc/libvirt/qemu/networks/default.xml`). `net-destroy` solo detiene la red (mata dnsmasq, elimina virbr0), no modifica el XML.
- **Leases dinamicos**: **SI se pierden**. Al detener dnsmasq, el archivo de leases se limpia. Al reiniciar, las VMs que sigan encendidas intentaran renovar su lease — si la VM pide la misma IP y esta disponible, normalmente la obtendra de nuevo, pero no esta garantizado.

**`net-undefine default`:**
- **Elimina la configuracion permanente** — el XML de la red se borra. Las reservas `<host>` desaparecen. Si la red estaba activa al hacer undefine, sigue corriendo hasta que se ejecute `net-destroy`, pero al detenerla no se podra reiniciar porque ya no existe la definicion.
- Para recrearla habria que usar `virsh net-define <archivo.xml>` con un nuevo XML.

Moraleja: `destroy` = apagar (reversible), `undefine` = eliminar (destructivo).

</details>

---

> **Nota**: DHCP es el protocolo que convierte la tarea manual de "configurar red en cada VM" en algo automatico y centralizado. En libvirt, dnsmasq se encarga de todo: asigna IPs, entrega gateway y DNS, y registra hostnames para resolucion. Las reservas DHCP son la herramienta clave para labs con VMs que necesitan IPs predecibles sin perder la ventaja de la configuracion centralizada. Antes de configurar IP estatica manualmente dentro de una VM, preguntate si una reserva DHCP no seria mas limpia.
