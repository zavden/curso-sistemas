# Red NAT por defecto

## Índice

1. [Redes virtuales de libvirt](#redes-virtuales-de-libvirt)
2. [La red default: arquitectura](#la-red-default-arquitectura)
3. [Componentes de la red default](#componentes-de-la-red-default)
4. [Inspeccionar la red default](#inspeccionar-la-red-default)
5. [virbr0: el bridge del host](#virbr0-el-bridge-del-host)
6. [DHCP y asignación de IPs](#dhcp-y-asignación-de-ips)
7. [NAT: cómo salen las VMs a internet](#nat-cómo-salen-las-vms-a-internet)
8. [DNS interno con dnsmasq](#dns-interno-con-dnsmasq)
9. [Cuándo es suficiente la red default](#cuándo-es-suficiente-la-red-default)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## Redes virtuales de libvirt

libvirt gestiona redes virtuales de la misma forma que gestiona pools de
almacenamiento: definición XML, estados (active/inactive), autostart. Las
VMs se conectan a estas redes virtuales en vez de directamente al hardware
de red del host.

```
┌──────────────────────────────────────────────────────────────────┐
│  HOST                                                            │
│                                                                  │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐                         │
│  │  VM 1   │  │  VM 2   │  │  VM 3   │                         │
│  │ .122.10 │  │ .122.11 │  │ .122.12 │                         │
│  └────┬────┘  └────┬────┘  └────┬────┘                         │
│       │            │            │                                │
│  ─────┴────────────┴────────────┴──────── Red virtual "default" │
│                     │                                            │
│              ┌──────┴──────┐                                    │
│              │   virbr0    │  Bridge virtual (192.168.122.1)    │
│              │   + dnsmasq │  DHCP + DNS + NAT                  │
│              └──────┬──────┘                                    │
│                     │ iptables NAT                               │
│              ┌──────┴──────┐                                    │
│              │   eth0/wlan │  Interfaz física del host           │
│              └──────┬──────┘                                    │
│                     │                                            │
└─────────────────────┼────────────────────────────────────────────┘
                      │
                 ── Internet ──
```

---

## La red default: arquitectura

Al instalar libvirt, se crea automáticamente una red llamada `default` con
estas características:

| Propiedad | Valor |
|-----------|-------|
| Nombre | `default` |
| Modo | NAT (network address translation) |
| Subred | `192.168.122.0/24` |
| Gateway | `192.168.122.1` (el host) |
| DHCP range | `192.168.122.2` — `192.168.122.254` |
| Bridge | `virbr0` |
| DNS | dnsmasq (resuelve nombres + forwarding) |
| Autostart | Sí |

Las VMs conectadas a esta red:

- **Pueden** salir a internet (a través del NAT del host)
- **Pueden** comunicarse entre sí (están en la misma subred)
- **Pueden** alcanzar al host (gateway 192.168.122.1)
- **No pueden** ser accedidas desde fuera del host directamente

```
┌────────────────────────────────────────────────┐
│                                                │
│  VM → Internet:  ✅ (NAT via host)             │
│  VM → VM:        ✅ (misma subred)              │
│  VM → Host:      ✅ (gateway .122.1)            │
│  Host → VM:      ✅ (SSH a la IP de la VM)      │
│  Externo → VM:   ❌ (NAT bloquea conexiones     │
│                      entrantes por defecto)     │
│                                                │
└────────────────────────────────────────────────┘
```

---

## Componentes de la red default

La red default se implementa con componentes del kernel Linux y un proceso
en userspace:

```
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│  1. BRIDGE: virbr0                                               │
│     Dispositivo de red virtual del kernel que conecta             │
│     las interfaces de las VMs (como un switch virtual).          │
│     IP del host en el bridge: 192.168.122.1/24                  │
│                                                                  │
│  2. DNSMASQ: proceso en userspace                                │
│     Proporciona DHCP, DNS y TFTP a las VMs.                     │
│     Escucha solo en virbr0 (no en la red física).               │
│     PID file: /var/run/libvirt/network/default.pid              │
│                                                                  │
│  3. IPTABLES/NFTABLES: reglas de NAT                             │
│     MASQUERADE para tráfico saliente de 192.168.122.0/24.       │
│     Permiten que las VMs accedan a internet a través del host.  │
│                                                                  │
│  4. vnetN: interfaces TAP                                        │
│     Cada VM tiene una interfaz tap (vnet0, vnet1, etc.)         │
│     conectada al bridge virbr0.                                 │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

La relación entre estos componentes:

```
VM1 (enp1s0) ←→ vnet0 ─┐
                        │
VM2 (enp1s0) ←→ vnet1 ─┼── virbr0 (bridge) ── dnsmasq (DHCP/DNS)
                        │        │
VM3 (enp1s0) ←→ vnet2 ─┘        │
                            iptables NAT
                                 │
                         interfaz física (eth0)
                                 │
                             Internet
```

---

## Inspeccionar la red default

### Estado de las redes

```bash
virsh net-list --all
```

```
 Name      State    Autostart   Persistent
--------------------------------------------
 default   active   yes         yes
```

### Información detallada

```bash
virsh net-info default
```

```
Name:           default
UUID:           a1b2c3d4-...
Active:         yes
Persistent:     yes
Autostart:      yes
Bridge:         virbr0
```

### XML de la red

```bash
virsh net-dumpxml default
```

```xml
<network>
  <name>default</name>
  <uuid>a1b2c3d4-e5f6-7890-abcd-ef1234567890</uuid>
  <forward mode='nat'>
    <nat>
      <port start='1024' end='65535'/>
    </nat>
  </forward>
  <bridge name='virbr0' stp='on' delay='0'/>
  <mac address='52:54:00:xx:xx:xx'/>
  <ip address='192.168.122.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='192.168.122.2' end='192.168.122.254'/>
    </dhcp>
  </ip>
</network>
```

Cada sección del XML:

| Elemento | Significado |
|----------|-------------|
| `<forward mode='nat'>` | Modo NAT: las VMs salen a internet via el host |
| `<port start='1024' end='65535'/>` | Rango de puertos para el MASQUERADE |
| `<bridge name='virbr0'>` | Nombre del bridge en el host |
| `stp='on'` | Spanning Tree Protocol activo (previene loops) |
| `<ip address='192.168.122.1'>` | IP del host en el bridge |
| `<dhcp><range .../>` | Rango de IPs que asigna el DHCP |

### Leases DHCP activos

```bash
virsh net-dhcp-leases default
```

```
 Expiry time           MAC address        Protocol  IP address         Hostname
----------------------------------------------------------------------------------
 2026-03-20 12:30:00   52:54:00:12:34:56  ipv4      192.168.122.10/24  debian-lab
 2026-03-20 12:31:00   52:54:00:78:9a:bc  ipv4      192.168.122.11/24  alma-lab
```

Esto muestra todas las VMs que han obtenido IP por DHCP en la red default.

---

## virbr0: el bridge del host

### Ver el bridge

```bash
ip addr show virbr0
```

```
5: virbr0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 state UP
    link/ether 52:54:00:xx:xx:xx brd ff:ff:ff:ff:ff:ff
    inet 192.168.122.1/24 brd 192.168.122.255 scope global virbr0
```

### Ver las interfaces conectadas al bridge

```bash
bridge link show dev virbr0
# o
ip link show master virbr0
```

```
7: vnet0: <BROADCAST,MULTICAST,UP> mtu 1500 master virbr0
8: vnet1: <BROADCAST,MULTICAST,UP> mtu 1500 master virbr0
```

Cada `vnetN` es la interfaz TAP de una VM. Cuando una VM se apaga, su
`vnetN` desaparece del bridge.

### Relación VM ↔ interfaz TAP

```bash
virsh domiflist debian-lab
```

```
 Interface   Type      Source    Model    MAC
--------------------------------------------------------------
 vnet0       network   default  virtio   52:54:00:12:34:56
```

`vnet0` en el host es la punta del túnel TAP que conecta `enp1s0` dentro
del guest al bridge `virbr0`.

```
Dentro de la VM:                   En el host:

enp1s0 ──────────────────────────► vnet0 ── virbr0
(192.168.122.10)   túnel TAP      (sin IP, es un puerto del bridge)
```

---

## DHCP y asignación de IPs

### Cómo funciona

Cuando una VM arranca con `--network network=default`:

```
1. VM arranca → guest ejecuta dhclient/NetworkManager
       │
2. DHCPDISCOVER broadcast en la subred
       │
3. dnsmasq (escuchando en virbr0) recibe la solicitud
       │
4. dnsmasq asigna IP del rango 192.168.122.2-254
       │
5. VM obtiene: IP, gateway (192.168.122.1), DNS (192.168.122.1)
```

### Reservar IP fija para una VM

Puedes asignar una IP específica basándote en el MAC address de la VM:

```bash
virsh net-update default add ip-dhcp-host \
    "<host mac='52:54:00:12:34:56' name='debian-lab' ip='192.168.122.100'/>" \
    --live --config
```

Esto le dice a dnsmasq: "cuando veas el MAC `52:54:00:12:34:56`, asigna
siempre la IP `192.168.122.100`".

Verificar:

```bash
virsh net-dumpxml default | grep -A1 "dhcp"
```

```xml
<dhcp>
  <range start='192.168.122.2' end='192.168.122.254'/>
  <host mac='52:54:00:12:34:56' name='debian-lab' ip='192.168.122.100'/>
</dhcp>
```

Para que tome efecto, la VM debe renovar su lease DHCP:

```bash
# Dentro de la VM:
sudo dhclient -r && sudo dhclient
# o reiniciar la VM
```

### Ver el archivo de leases

```bash
cat /var/lib/libvirt/dnsmasq/virbr0.status
```

```json
[
  {
    "ip-address": "192.168.122.10",
    "mac-address": "52:54:00:12:34:56",
    "hostname": "debian-lab",
    "client-id": "...",
    "expiry-time": 1710936600
  }
]
```

---

## NAT: cómo salen las VMs a internet

### El mecanismo

NAT (Network Address Translation) permite que las VMs con IPs privadas
(192.168.122.x) accedan a internet a través de la IP pública del host:

```
VM (192.168.122.10) quiere acceder a deb.debian.org:

1. VM envía paquete: src=192.168.122.10 dst=deb.debian.org
       │
2. Paquete llega a virbr0 (gateway 192.168.122.1)
       │
3. iptables MASQUERADE: reemplaza src=192.168.122.10 por src=IP_del_host
       │
4. Paquete sale por la interfaz física del host
       │
5. Respuesta llega al host
       │
6. iptables des-NAT: reemplaza dst=IP_del_host por dst=192.168.122.10
       │
7. Paquete llega a la VM
```

### Ver las reglas de iptables

```bash
sudo iptables -t nat -L -n | grep 122
```

```
Chain POSTROUTING
MASQUERADE  all  --  192.168.122.0/24  !192.168.122.0/24
```

Esta regla dice: "para todo tráfico que viene de 192.168.122.0/24 y va
hacia fuera de esa subred, aplicar MASQUERADE (NAT)".

Con nftables (sistemas más recientes):

```bash
sudo nft list ruleset | grep 122
```

### Limitación del NAT: sin acceso entrante

El NAT solo funciona para conexiones **salientes** (iniciadas por la VM).
Conexiones entrantes desde fuera del host no pueden alcanzar las VMs
directamente:

```
Internet → Host:22    ✅  (SSH al host)
Internet → VM:22      ❌  (la IP 192.168.122.x no es routable)
```

Para permitir acceso entrante a una VM hay dos opciones (que veremos
en temas posteriores):

1. **Port forwarding**: redirigir un puerto del host a la VM
2. **Bridge**: conectar la VM directamente a la red física (T03)

---

## DNS interno con dnsmasq

El proceso dnsmasq que libvirt lanza para la red default también funciona
como **servidor DNS** para las VMs:

### Resolución de nombres entre VMs

Cuando una VM obtiene IP por DHCP, dnsmasq registra su hostname. Otras
VMs pueden resolver ese nombre:

```bash
# Dentro de VM2:
ping debian-lab
# PING debian-lab (192.168.122.10) ...

# Esto funciona porque dnsmasq conoce el hostname de debian-lab
# (lo aprendió del DHCPREQUEST)
```

### Forwarding DNS

dnsmasq también reenvía consultas DNS que no puede resolver localmente
al DNS configurado en el host:

```bash
# Dentro de la VM:
dig google.com
# Responde correctamente — dnsmasq reenvia al DNS del host
```

### Ver el proceso dnsmasq

```bash
ps aux | grep "[d]nsmasq.*virbr0"
```

```
nobody  1234  ... /usr/sbin/dnsmasq --conf-file=/var/lib/libvirt/dnsmasq/default.conf
```

```bash
cat /var/lib/libvirt/dnsmasq/default.conf
```

```
strict-order
pid-file=/var/run/libvirt/network/default.pid
except-interface=lo
bind-dynamic
interface=virbr0
dhcp-range=192.168.122.2,192.168.122.254,255.255.255.0
dhcp-no-override
dhcp-authoritative
dhcp-lease-max=253
dhcp-hostsfile=/var/lib/libvirt/dnsmasq/default.hostsfile
addn-hosts=/var/lib/libvirt/dnsmasq/default.addnhosts
```

---

## Cuándo es suficiente la red default

La red default NAT es adecuada para la mayoría de nuestros labs:

| Escenario | ¿Default es suficiente? |
|-----------|------------------------|
| VM individual que necesita internet | ✅ Sí |
| Instalar paquetes (`apt`, `dnf`) | ✅ Sí |
| Varias VMs que se comunican entre sí | ✅ Sí |
| SSH desde el host a la VM | ✅ Sí |
| Lab de filesystems, LVM, RAID | ✅ Sí |
| Lab de DNS/DHCP (ser servidor DHCP) | ❌ No (conflicto con dnsmasq de la default) |
| Lab de routing entre subnets | ❌ No (necesitas redes aisladas) |
| Lab de firewall | ❌ No (necesitas redes aisladas) |
| Acceder a la VM desde otro PC | ❌ No (necesitas bridge) |

Para los casos donde default no es suficiente, usaremos redes aisladas
(T02) o bridges (T03).

> **Predicción**: para la gran mayoría de los labs del curso (B08
> almacenamiento, particionado, LVM, RAID, filesystems), la red default
> es todo lo que necesitas. Las redes aisladas y bridges serán esenciales
> en B09 (Redes) y B10 (Servicios).

---

## Errores comunes

### 1. Red default no activa

```bash
# ❌ VM no obtiene red
virsh net-list --all
# default  inactive  no

# ✅ Arrancar y configurar autostart
virsh net-start default
virsh net-autostart default
```

Causas comunes de que la red default esté inactiva:
- Nunca se inició después de instalar libvirt
- Se detuvo manualmente con `virsh net-stop`
- Conflicto de subred con otra interfaz del host

### 2. Conflicto de subred

```bash
# ❌ El host ya tiene una interfaz en 192.168.122.0/24
ip addr | grep 192.168.122
# inet 192.168.122.50/24 ... eth1

virsh net-start default
# error: internal error: Network is already in use by interface eth1
```

**Solución**: cambiar la subred de la red default:

```bash
virsh net-edit default
```

Cambiar:

```xml
<ip address='192.168.122.1' netmask='255.255.255.0'>
```

Por:

```xml
<ip address='192.168.100.1' netmask='255.255.255.0'>
  <dhcp>
    <range start='192.168.100.2' end='192.168.100.254'/>
  </dhcp>
</ip>
```

```bash
virsh net-stop default
virsh net-start default
```

### 3. VM sin red por firewall del host

```bash
# ❌ La VM obtiene IP pero no sale a internet
# Dentro de la VM:
ping 192.168.122.1    # ✅ funciona (gateway)
ping 8.8.8.8          # ❌ falla (sin NAT)

# Causa: firewalld o ufw en el host bloquean el forwarding
# Verificar:
sudo sysctl net.ipv4.ip_forward
# net.ipv4.ip_forward = 0  ← ¡deshabilitado!

# ✅ Solución:
sudo sysctl -w net.ipv4.ip_forward=1
# libvirt normalmente habilita esto automáticamente,
# pero firewalld puede sobreescribirlo
```

En Fedora con firewalld:

```bash
sudo firewall-cmd --zone=libvirt --list-all
# Si la zona libvirt no existe o no permite masquerade:
sudo firewall-cmd --zone=libvirt --add-masquerade --permanent
sudo firewall-cmd --reload
```

### 4. domifaddr no muestra IP

```bash
virsh domifaddr debian-lab
# (vacío)

# Alternativa 1: buscar en los leases DHCP
virsh net-dhcp-leases default

# Alternativa 2: la VM acaba de arrancar, DHCP no ha respondido
# Esperar unos segundos

# Alternativa 3: la VM usa IP estática (no pasó por DHCP)
# Conectar por virsh console y verificar con: ip addr
```

### 5. Intentar borrar la red default con VMs conectadas

```bash
# ❌ VMs usando la red
virsh net-destroy default
# Funciona (desactiva la red), pero las VMs pierden conectividad
# Las VMs siguen corriendo pero sin red

# ✅ Primero apagar las VMs o reconectar a otra red
```

---

## Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║  RED NAT DEFAULT — REFERENCIA RÁPIDA                               ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  GESTIÓN DE REDES:                                                 ║
║    virsh net-list --all              listar todas las redes        ║
║    virsh net-info default            info de la red default        ║
║    virsh net-dumpxml default         XML completo                  ║
║    virsh net-start default           arrancar red                  ║
║    virsh net-autostart default       arrancar al boot              ║
║    virsh net-stop default            detener red (¡VMs sin red!)   ║
║    virsh net-edit default            editar XML                    ║
║                                                                    ║
║  CONSULTAR IPs:                                                    ║
║    virsh net-dhcp-leases default     leases DHCP activos           ║
║    virsh domifaddr VM                IP de una VM específica       ║
║                                                                    ║
║  RESERVAR IP FIJA:                                                 ║
║    virsh net-update default add ip-dhcp-host \                     ║
║      "<host mac='XX:XX' ip='192.168.122.100'/>" \                  ║
║      --live --config                                               ║
║                                                                    ║
║  RED DEFAULT = 192.168.122.0/24:                                   ║
║    Gateway:    192.168.122.1 (el host, virbr0)                     ║
║    DHCP:       192.168.122.2 — .254                                ║
║    DNS:        192.168.122.1 (dnsmasq)                             ║
║    Bridge:     virbr0                                              ║
║    NAT:        saliente ✅  entrante ❌                              ║
║                                                                    ║
║  VERIFICAR CONECTIVIDAD (dentro de la VM):                         ║
║    ip addr                   ver IP asignada                       ║
║    ping 192.168.122.1        gateway accesible                     ║
║    ping 8.8.8.8              internet (NAT funciona)               ║
║    ping debian-lab           resolución entre VMs (dnsmasq)        ║
║                                                                    ║
║  VERIFICAR EN EL HOST:                                             ║
║    ip addr show virbr0         IP del bridge                       ║
║    bridge link show            interfaces TAP conectadas           ║
║    ps aux | grep dnsmasq       proceso DHCP/DNS                    ║
║    sudo iptables -t nat -L -n  reglas NAT                         ║
║    sysctl net.ipv4.ip_forward  forwarding habilitado               ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Explorar la red default

**Objetivo**: entender todos los componentes de la red default sin
modificar nada.

1. Verifica que la red default está activa:
   ```bash
   virsh net-list --all
   virsh net-info default
   ```

2. Examina el XML completo:
   ```bash
   virsh net-dumpxml default
   ```
   Identifica: modo (`nat`), bridge (`virbr0`), subred, rango DHCP.

3. Inspecciona el bridge en el host:
   ```bash
   ip addr show virbr0
   ip link show master virbr0
   ```
   ¿Tiene interfaces TAP conectadas? (Solo si hay VMs corriendo.)

4. Encuentra el proceso dnsmasq:
   ```bash
   ps aux | grep "[d]nsmasq.*virbr0"
   ```
   ¿Con qué usuario corre? ¿Qué archivo de configuración usa?

5. Verifica las reglas NAT:
   ```bash
   sudo iptables -t nat -L -n -v | grep 122
   # o con nftables:
   sudo nft list ruleset | grep 122
   ```

6. Verifica ip_forward:
   ```bash
   sysctl net.ipv4.ip_forward
   ```
   ¿Está habilitado? Si no, ¿quién debería habilitarlo?

7. Si tienes VMs corriendo, consulta los leases:
   ```bash
   virsh net-dhcp-leases default
   ```

**Pregunta de reflexión**: la red default usa la subred 192.168.122.0/24.
¿Qué pasaría si tu red doméstica también usa 192.168.122.0/24? ¿Cómo lo
solucionarías?

---

### Ejercicio 2: Comunicación entre VMs

**Objetivo**: verificar que dos VMs en la red default pueden comunicarse.

1. Crea dos VMs (o usa las existentes):
   ```bash
   sudo ./create-lab-vm.sh vm-alpha 0
   sudo ./create-lab-vm.sh vm-beta 0
   sleep 30
   ```

2. Obtén las IPs de ambas:
   ```bash
   virsh net-dhcp-leases default
   ```
   Anota: VM-alpha = _______, VM-beta = _______

3. Desde vm-alpha, intenta contactar vm-beta:
   ```bash
   virsh console vm-alpha
   ping -c 3 IP_DE_BETA
   # ¿Funciona?
   ```

4. Prueba la resolución DNS entre VMs:
   ```bash
   # Dentro de vm-alpha:
   ping -c 3 vm-beta
   # ¿dnsmasq resuelve el hostname?
   ```

5. Prueba el acceso a internet:
   ```bash
   # Dentro de vm-alpha:
   ping -c 3 8.8.8.8
   ping -c 3 google.com
   ```

6. Verifica la ruta de red dentro de la VM:
   ```bash
   ip route
   # default via 192.168.122.1 dev enp1s0
   # 192.168.122.0/24 dev enp1s0 proto kernel
   ```

7. Desde el host, verifica las interfaces TAP:
   ```bash
   ip link show master virbr0
   virsh domiflist vm-alpha
   virsh domiflist vm-beta
   ```

8. Limpia:
   ```bash
   sudo ./destroy-lab-vm.sh vm-alpha
   sudo ./destroy-lab-vm.sh vm-beta
   ```

**Pregunta de reflexión**: las dos VMs se comunican por la subred
192.168.122.0/24 a través del bridge virbr0. ¿El tráfico entre ellas
sale al exterior (a la interfaz física del host) o se queda dentro del
bridge? ¿Cómo podrías verificarlo?

---

### Ejercicio 3: Reservar IP fija y probar DNS

**Objetivo**: asignar una IP fija a una VM y verificar la resolución DNS.

1. Crea una VM:
   ```bash
   sudo ./create-lab-vm.sh vm-fixed 0
   sleep 20
   ```

2. Obtén el MAC address de la VM:
   ```bash
   virsh domiflist vm-fixed
   # Anota el MAC: 52:54:00:xx:xx:xx
   ```

3. Reserva la IP 192.168.122.200:
   ```bash
   sudo virsh net-update default add ip-dhcp-host \
       "<host mac='52:54:00:xx:xx:xx' name='vm-fixed' ip='192.168.122.200'/>" \
       --live --config
   ```

4. Fuerza la renovación DHCP dentro de la VM:
   ```bash
   virsh console vm-fixed
   sudo dhclient -r && sudo dhclient
   # o: sudo systemctl restart NetworkManager
   ip addr show
   ```
   ¿Tiene la IP 192.168.122.200?

5. Verifica la reserva en el XML:
   ```bash
   virsh net-dumpxml default | grep -A1 host
   ```

6. Reinicia la VM y verifica que conserva la misma IP:
   ```bash
   virsh reboot vm-fixed
   sleep 15
   virsh domifaddr vm-fixed
   # ¿Sigue siendo 192.168.122.200?
   ```

7. Elimina la reserva cuando termines:
   ```bash
   sudo virsh net-update default delete ip-dhcp-host \
       "<host mac='52:54:00:xx:xx:xx' name='vm-fixed' ip='192.168.122.200'/>" \
       --live --config
   ```

8. Limpia:
   ```bash
   sudo ./destroy-lab-vm.sh vm-fixed
   ```

**Pregunta de reflexión**: ¿por qué es útil tener IPs fijas para VMs de
lab? Si tuvieras un script que conecta por SSH a una VM, ¿sería más
fiable con DHCP dinámico o con IP reservada?
