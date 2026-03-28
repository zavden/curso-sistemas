# Red bridge (puente)

## Índice

1. [Qué es un bridge de red](#qué-es-un-bridge-de-red)
2. [Bridge vs NAT: cuándo usar cada uno](#bridge-vs-nat-cuándo-usar-cada-uno)
3. [Arquitectura del bridge](#arquitectura-del-bridge)
4. [Crear un bridge con nmcli (Fedora / RHEL)](#crear-un-bridge-con-nmcli-fedora--rhel)
5. [Crear un bridge con interfaces (Debian)](#crear-un-bridge-con-interfaces-debian)
6. [Usar el bridge en libvirt](#usar-el-bridge-en-libvirt)
7. [Verificar el funcionamiento](#verificar-el-funcionamiento)
8. [Bridge en portátiles con WiFi](#bridge-en-portátiles-con-wifi)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## Qué es un bridge de red

Un bridge (puente) conecta las VMs directamente a la **red física** del
host. Las VMs obtienen IPs del router/DHCP de tu red real y son visibles
como si fueran máquinas físicas adicionales en esa red.

```
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│  Con NAT (default):               Con bridge:                    │
│                                                                  │
│  VMs en 192.168.122.x             VMs en 192.168.1.x            │
│  (red privada virtual)            (misma red que el host)        │
│                                                                  │
│  Internet ve solo el host         Internet ve VMs como PCs       │
│  VMs ocultas detrás del NAT       VMs accesibles directamente   │
│                                                                  │
│  Router doméstico:                Router doméstico:              │
│    192.168.1.1                      192.168.1.1                  │
│    └── Host: 192.168.1.100          ├── Host: 192.168.1.100     │
│        (VMs invisibles)             ├── VM1:  192.168.1.101     │
│                                     └── VM2:  192.168.1.102     │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## Bridge vs NAT: cuándo usar cada uno

| Aspecto | NAT (default) | Bridge |
|---------|--------------|--------|
| VMs accesibles desde la LAN | ❌ No | ✅ Sí |
| VMs accesibles desde internet | ❌ No | Depende del router |
| Configuración | Automática (libvirt) | Manual en el host |
| IP de las VMs | Red privada 192.168.122.x | Red real del host |
| DHCP | dnsmasq de libvirt | Router real de la red |
| Rendimiento de red | Ligeramente menor (NAT overhead) | Nativo |
| Funciona con WiFi | ✅ Siempre | ⚠ Problemático |
| Independiente de la red física | ✅ Sí | ❌ No |
| Uso principal | Labs, desarrollo | Servidores, servicios expuestos |

**Para el curso**: la red NAT default es suficiente para la mayoría de labs.
El bridge es útil si necesitas acceder a las VMs desde otro PC de tu red
(por ejemplo, probar un servicio web desde tu teléfono o desde otro
ordenador).

---

## Arquitectura del bridge

Un bridge de red funciona como un **switch virtual** en capa 2. La interfaz
física del host se convierte en un "puerto" del switch, al igual que las
interfaces TAP de las VMs:

```
┌──────────────────────────────────────────────────────────────────┐
│  HOST                                                            │
│                                                                  │
│  ┌─────────┐  ┌─────────┐                                      │
│  │  VM 1   │  │  VM 2   │                                      │
│  │  .1.101 │  │  .1.102 │                                      │
│  └────┬────┘  └────┬────┘                                      │
│       │            │                                             │
│     vnet0        vnet1        eth0 (sin IP propia)              │
│       │            │            │                                │
│  ─────┴────────────┴────────────┴─── br0 (bridge)               │
│                                      IP: 192.168.1.100          │
│                                        │                        │
│                                  ┌─────┴─────┐                 │
│                                  │ eth0 (HW)  │                 │
│                                  └─────┬─────┘                 │
│                                        │                        │
└────────────────────────────────────────┼────────────────────────┘
                                         │
                                    ┌────┴────┐
                                    │ Switch / │
                                    │ Router   │
                                    │.1.1 DHCP │
                                    └─────────┘
                                         │
                                    ── Internet ──
```

Lo clave: la interfaz física `eth0` **pierde su IP** y se convierte en un
puerto del bridge. El bridge `br0` **hereda la IP** que antes tenía `eth0`.
Todo el tráfico del host sale ahora a través de `br0`.

> **Advertencia**: configurar un bridge en una conexión remota (SSH) es
> arriesgado. Si algo sale mal, pierdes la conectividad. Hazlo con acceso
> físico o por consola, o prepara un script que revierta los cambios
> automáticamente si falla.

---

## Crear un bridge con nmcli (Fedora / RHEL)

NetworkManager es el gestor de red estándar en Fedora, RHEL y AlmaLinux.
Usamos `nmcli` para crear el bridge.

### Paso 1: Identificar la interfaz física

```bash
nmcli device status
```

```
DEVICE   TYPE      STATE      CONNECTION
eth0     ethernet  connected  Wired connection 1
lo       loopback  unmanaged  --
```

Anota el nombre de tu interfaz (`eth0`, `enp1s0`, `eno1`, etc.) y el nombre
de la conexión (`Wired connection 1`).

### Paso 2: Crear el bridge

```bash
# Crear el bridge
sudo nmcli connection add type bridge ifname br0 con-name br0

# Configurar IP (estática recomendada para servidores)
sudo nmcli connection modify br0 \
    ipv4.addresses 192.168.1.100/24 \
    ipv4.gateway 192.168.1.1 \
    ipv4.dns "192.168.1.1 8.8.8.8" \
    ipv4.method manual

# O usar DHCP (más simple pero la IP puede cambiar)
sudo nmcli connection modify br0 ipv4.method auto
```

### Paso 3: Añadir la interfaz física al bridge

```bash
# Esclava: la interfaz física se convierte en puerto del bridge
sudo nmcli connection add type bridge-slave ifname eth0 master br0
```

### Paso 4: Activar

```bash
# Desactivar la conexión antigua
sudo nmcli connection down "Wired connection 1"

# Activar el bridge
sudo nmcli connection up br0
```

> **Momento crítico**: entre `down` y `up` pierdes conectividad de red
> momentáneamente. Si estás por SSH, ejecuta ambos comandos concatenados:
> ```bash
> sudo nmcli connection down "Wired connection 1" && sudo nmcli connection up br0
> ```

### Paso 5: Verificar

```bash
nmcli device status
```

```
DEVICE   TYPE      STATE      CONNECTION
br0      bridge    connected  br0
eth0     ethernet  connected  bridge-slave-eth0
lo       loopback  unmanaged  --
```

```bash
ip addr show br0
```

```
10: br0: <BROADCAST,MULTICAST,UP> mtu 1500 state UP
    inet 192.168.1.100/24 brd 192.168.1.255 scope global br0
```

```bash
# eth0 ya no tiene IP propia
ip addr show eth0 | grep inet
# (sin resultado — correcto)

# Verificar que el bridge tiene a eth0 como puerto
bridge link show
```

```
3: eth0: <BROADCAST,MULTICAST,UP> mtu 1500 master br0
```

---

## Crear un bridge con interfaces (Debian)

En Debian (sin NetworkManager), se configura en
`/etc/network/interfaces`:

### Instalar bridge-utils

```bash
sudo apt install bridge-utils
```

### Configurar

```bash
sudo vim /etc/network/interfaces
```

```
# Loopback
auto lo
iface lo inet loopback

# Interfaz física (sin IP, como puerto del bridge)
auto eth0
iface eth0 inet manual

# Bridge
auto br0
iface br0 inet static
    address 192.168.1.100
    netmask 255.255.255.0
    gateway 192.168.1.1
    dns-nameservers 192.168.1.1 8.8.8.8
    bridge_ports eth0
    bridge_stp on
    bridge_fd 0
```

O con DHCP:

```
auto br0
iface br0 inet dhcp
    bridge_ports eth0
    bridge_stp on
    bridge_fd 0
```

### Activar

```bash
sudo systemctl restart networking
```

O si estás por SSH:

```bash
sudo ifdown eth0 && sudo ifup br0
```

### Con Netplan (Ubuntu)

```yaml
# /etc/netplan/01-bridge.yaml
network:
  version: 2
  renderer: networkd
  ethernets:
    eth0:
      dhcp4: false
  bridges:
    br0:
      interfaces: [eth0]
      dhcp4: true
      # O estática:
      # addresses: [192.168.1.100/24]
      # gateway4: 192.168.1.1
```

```bash
sudo netplan apply
```

---

## Usar el bridge en libvirt

Una vez que el bridge `br0` existe en el host, las VMs se conectan a él
directamente, **sin pasar por una red virtual de libvirt**.

### En virt-install

```bash
sudo virt-install \
    --name vm-bridged \
    --ram 2048 --vcpus 2 --import \
    --disk path=/var/lib/libvirt/images/vm-bridged.qcow2 \
    --os-variant debian12 \
    --network bridge=br0,model=virtio \
    --graphics none --noautoconsole
```

El flag `--network bridge=br0` en vez de `--network network=default`.

### En el XML de la VM

```xml
<interface type='bridge'>
  <source bridge='br0'/>
  <model type='virtio'/>
</interface>
```

Compara con la red NAT:

```xml
<!-- NAT (red virtual de libvirt) -->
<interface type='network'>
  <source network='default'/>
  <model type='virtio'/>
</interface>

<!-- Bridge (bridge del host directamente) -->
<interface type='bridge'>
  <source bridge='br0'/>
  <model type='virtio'/>
</interface>
```

La diferencia clave: `type='network'` pasa por libvirt (que gestiona el
bridge, DHCP, NAT). `type='bridge'` usa el bridge del host directamente
(libvirt no gestiona DHCP ni NAT para este caso).

### Conectar a VM existente

```bash
# Agregar interfaz bridge a VM existente
sudo virsh attach-interface debian-lab bridge br0 \
    --model virtio --persistent
```

### Qué pasa dentro de la VM

La VM obtiene IP del **DHCP de tu red real** (tu router doméstico, tu
servidor DHCP corporativo, etc.), no del dnsmasq de libvirt:

```bash
# Dentro de la VM:
ip addr show enp1s0
# inet 192.168.1.101/24  ← IP de tu red real

# El gateway es tu router real:
ip route
# default via 192.168.1.1 dev enp1s0
```

Desde cualquier otro dispositivo en tu red:

```bash
# Desde otro PC en la misma red:
ping 192.168.1.101    # ✅ la VM es alcanzable
ssh labuser@192.168.1.101  # ✅ acceso SSH directo
```

---

## Verificar el funcionamiento

### En el host

```bash
# Ver los puertos del bridge
bridge link show
```

```
3: eth0: <BROADCAST,MULTICAST,UP> master br0
7: vnet0: <BROADCAST,MULTICAST,UP> master br0    ← VM1
8: vnet1: <BROADCAST,MULTICAST,UP> master br0    ← VM2
```

```bash
# Ver la tabla de MACs del bridge (qué MAC está en qué puerto)
bridge fdb show br0 | head -10
```

### Dentro de la VM

```bash
# Verificar que la IP es de la red real
ip addr show

# Verificar que el gateway es el router real
ip route show

# Verificar conectividad
ping -c 2 192.168.1.1     # router real
ping -c 2 8.8.8.8         # internet
ping -c 2 192.168.1.100   # el host
```

### Desde otro dispositivo de la red

```bash
# Desde un laptop/PC en la misma red:
ping 192.168.1.101         # la VM
ssh labuser@192.168.1.101  # acceso directo
```

---

## Bridge en portátiles con WiFi

Los bridges **no funcionan bien con WiFi**. El estándar 802.11 no soporta
que un cliente WiFi transmita tramas con MAC addresses diferentes a la
suya propia. Cuando haces bridge de una interfaz WiFi, las VMs no pueden
comunicarse correctamente.

```
┌────────────────────────────────────────────────────────┐
│                                                        │
│  Ethernet (cable):    Bridge funciona ✅                │
│    eth0 → br0 → VMs obtienen IPs reales               │
│                                                        │
│  WiFi:                Bridge NO funciona ❌              │
│    wlan0 → br0 → VMs no pueden transmitir frames      │
│    (802.11 rechaza frames con MAC distinto al cliente) │
│                                                        │
└────────────────────────────────────────────────────────┘
```

### Alternativas para WiFi

**Opción 1: usar la red NAT default** (la solución más simple)

La red NAT funciona independientemente de si el host usa WiFi o Ethernet.
Las VMs salen a internet vía NAT y no necesitan bridge.

**Opción 2: macvtap** (bridge virtual sobre WiFi)

```xml
<interface type='direct'>
  <source dev='wlan0' mode='bridge'/>
  <model type='virtio'/>
</interface>
```

macvtap crea una interfaz virtual que comparte la interfaz física. Funciona
parcialmente con WiFi pero tiene limitaciones: la VM no puede comunicarse
con el host (solo con el exterior).

**Opción 3: routing manual**

Configurar el host como router entre la red NAT de las VMs y la red WiFi.
Requiere iptables/nftables y es más complejo.

> **Recomendación para el curso**: si usas WiFi, quédate con la red NAT
> default. El bridge es para hosts con conexión por cable (servidores,
> desktops).

---

## Errores comunes

### 1. Perder conectividad al crear el bridge

```bash
# ❌ Quitar IP de eth0 sin tener br0 listo
sudo ip addr del 192.168.1.100/24 dev eth0
# ¡Desconectado! No puedes crear el bridge ahora.

# ✅ Usar nmcli/ifupdown que hacen la transición atómica
sudo nmcli connection down "Wired" && sudo nmcli connection up br0
```

Si pierdes conectividad:
- Acceso físico: conectar monitor y teclado
- Consola serial del servidor (IPMI/iLO/iDRAC)
- Reboot: la configuración anterior debería restaurarse si no guardaste

### 2. Bridge con WiFi

```bash
# ❌ Intentar crear bridge con wlan0
sudo nmcli connection add type bridge-slave ifname wlan0 master br0
# Error: WiFi no soporta bridge

# ✅ Usar NAT en su lugar
virt-install ... --network network=default
```

### 3. IP duplicada entre host y VM

```bash
# ❌ El host tiene 192.168.1.100 y la VM obtiene 192.168.1.100 por DHCP
# Conflicto de IP — ambos pierden conectividad intermitentemente

# ✅ Reservar IP fija para el host en el router/DHCP de tu red
# O configurar el host con IP estática fuera del rango DHCP
```

### 4. Olvidar model=virtio en el bridge

```bash
# ❌ Sin modelo especificado — usa e1000 emulado (lento)
virt-install ... --network bridge=br0

# ✅ Especificar virtio
virt-install ... --network bridge=br0,model=virtio
```

### 5. Firewall del host bloquea tráfico del bridge

```bash
# ❌ firewalld bloquea tráfico que cruza el bridge
# Las VMs no obtienen IP del DHCP o no pueden comunicarse

# Verificar:
sudo sysctl net.bridge.bridge-nf-call-iptables
# 1  ← iptables procesa tráfico del bridge (puede bloquear)

# ✅ Opción A: desactivar filtrado de bridge
sudo sysctl -w net.bridge.bridge-nf-call-iptables=0
sudo sysctl -w net.bridge.bridge-nf-call-ip6tables=0

# ✅ Opción B: añadir zona de firewalld para el bridge
sudo firewall-cmd --zone=trusted --add-interface=br0 --permanent
sudo firewall-cmd --reload
```

---

## Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║  RED BRIDGE — REFERENCIA RÁPIDA                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CREAR BRIDGE (Fedora/RHEL — nmcli):                               ║
║    nmcli con add type bridge ifname br0 con-name br0               ║
║    nmcli con modify br0 ipv4.addresses IP/24 ipv4.gateway GW \    ║
║      ipv4.dns DNS ipv4.method manual                               ║
║    nmcli con add type bridge-slave ifname eth0 master br0          ║
║    nmcli con down "Wired" && nmcli con up br0                     ║
║                                                                    ║
║  CREAR BRIDGE (Debian — /etc/network/interfaces):                  ║
║    auto br0                                                        ║
║    iface br0 inet dhcp   (o static con address/gateway/dns)       ║
║        bridge_ports eth0                                           ║
║        bridge_stp on                                               ║
║                                                                    ║
║  USAR EN VM:                                                       ║
║    virt-install ... --network bridge=br0,model=virtio              ║
║    virsh attach-interface VM bridge br0 --model virtio             ║
║                                                                    ║
║  XML:                                                              ║
║    <interface type='bridge'>                                       ║
║      <source bridge='br0'/>                                        ║
║      <model type='virtio'/>                                        ║
║    </interface>                                                     ║
║                                                                    ║
║  VERIFICAR:                                                        ║
║    bridge link show              puertos del bridge                ║
║    ip addr show br0              IP del bridge                     ║
║    nmcli device status           estado de dispositivos            ║
║                                                                    ║
║  REGLAS:                                                           ║
║    • Solo funciona con Ethernet (cable), NO con WiFi               ║
║    • La interfaz física pierde su IP (la hereda br0)               ║
║    • VMs obtienen IP del DHCP real de tu red                       ║
║    • Configurar con acceso físico o script de rollback             ║
║    • Especificar model=virtio siempre                              ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Entender la diferencia sin crear un bridge real

**Objetivo**: analizar la diferencia entre NAT y bridge comparando XMLs
y comportamientos, sin necesidad de modificar la red del host.

1. Examina el XML de una VM con red NAT:
   ```bash
   virsh dumpxml debian-lab | grep -A5 "interface"
   ```
   Identifica: `type='network'`, `source network='default'`.

2. Compara con el XML que tendrías con bridge:
   ```xml
   <interface type='bridge'>
     <source bridge='br0'/>
     <model type='virtio'/>
   </interface>
   ```
   ¿Qué diferencias ves? ¿Quién gestiona el DHCP en cada caso?

3. Verifica qué bridges existen actualmente en tu host:
   ```bash
   bridge link show
   ip link show type bridge
   ```
   ¿Ves `virbr0`? Es el bridge de la red NAT default.

4. Examina la diferencia de iptables entre una red NAT y un bridge:
   ```bash
   # Reglas NAT de la red default:
   sudo iptables -t nat -L -n | grep 122
   # MASQUERADE para 192.168.122.0/24

   # Un bridge real (br0) no tendría reglas NAT
   # Las VMs saldrían con su propia IP sin traducción
   ```

5. Dentro de una VM con red NAT, verifica:
   ```bash
   virsh console debian-lab
   ip route
   # default via 192.168.122.1  ← gateway es virbr0 (libvirt)

   # Con bridge sería:
   # default via 192.168.1.1    ← gateway es el router real
   ```

**Pregunta de reflexión**: si tu host tiene IP `192.168.1.100` y creas un
bridge, las VMs obtienen IPs como `192.168.1.101`, `192.168.1.102` del
router. ¿Qué pasa si tu router solo tiene 5 IPs disponibles en su pool
DHCP y creas 10 VMs?

---

### Ejercicio 2: Crear un bridge (solo con acceso físico o VM anidada)

**Objetivo**: practicar la creación de un bridge. Realiza este ejercicio
**solo** si tienes acceso físico al host o estás trabajando dentro de una
VM anidada (VM dentro de VM).

> **Advertencia**: no hagas este ejercicio por SSH en un servidor remoto
> sin acceso a consola de emergencia (IPMI, iLO, etc.).

1. Identifica tu interfaz de red:
   ```bash
   nmcli device status
   # o
   ip link show | grep "state UP"
   ```

2. Anota tu configuración actual (para poder revertir):
   ```bash
   ip addr show
   ip route show
   cat /etc/resolv.conf
   # Guardar en un archivo:
   nmcli connection show "Wired connection 1" > /tmp/backup-net.txt
   ```

3. Crea el bridge:
   ```bash
   sudo nmcli connection add type bridge ifname br0 con-name br0
   sudo nmcli connection modify br0 ipv4.method auto   # DHCP
   sudo nmcli connection add type bridge-slave ifname eth0 master br0
   ```

4. Activa (momento crítico):
   ```bash
   sudo nmcli connection down "Wired connection 1" && \
       sudo nmcli connection up br0
   ```

5. Verifica:
   ```bash
   ip addr show br0
   bridge link show
   ping -c 2 8.8.8.8
   ```

6. Crea una VM con bridge:
   ```bash
   sudo qemu-img create -f qcow2 \
       -b /var/lib/libvirt/images/tpl-debian12.qcow2 -F qcow2 \
       /var/lib/libvirt/images/vm-bridge-test.qcow2
   sudo virt-install --name vm-bridge-test --ram 512 --vcpus 1 --import \
       --disk path=/var/lib/libvirt/images/vm-bridge-test.qcow2 \
       --os-variant debian12 \
       --network bridge=br0,model=virtio \
       --graphics none --noautoconsole
   sleep 30
   ```

7. Verifica que la VM tiene IP de tu red real:
   ```bash
   virsh console vm-bridge-test
   ip addr show
   # ¿La IP es del rango de tu red real (no 192.168.122.x)?
   ```

8. Desde otro dispositivo en tu red, intenta hacer ping a la VM.

9. Revertir al estado original:
   ```bash
   sudo virsh destroy vm-bridge-test
   sudo virsh undefine vm-bridge-test --remove-all-storage
   sudo nmcli connection down br0
   sudo nmcli connection delete br0
   sudo nmcli connection delete bridge-slave-eth0
   sudo nmcli connection up "Wired connection 1"
   ```

**Pregunta de reflexión**: durante el paso 4, hubo un momento de
desconexión. Si fueras administrador de un servidor en producción,
¿cómo minimizarías el downtime al configurar un bridge? ¿Es posible
hacerlo con cero downtime?

---

### Ejercicio 3: Comparar rendimiento NAT vs bridge

**Objetivo**: entender las diferencias de rendimiento teóricas.

> **Nota**: este ejercicio es conceptual — no requiere un bridge real.

1. Analiza la ruta de un paquete con NAT:
   ```
   VM → vnet0 → virbr0 → iptables NAT → eth0 → Internet
   (6 pasos, traducción de direcciones)
   ```

2. Analiza la ruta con bridge:
   ```
   VM → vnet0 → br0 → eth0 → Internet
   (4 pasos, sin traducción)
   ```

3. ¿Dónde está el overhead del NAT?
   - iptables debe inspeccionar cada paquete
   - Debe reescribir headers (source IP/port)
   - Debe mantener la tabla conntrack (seguimiento de conexiones)

4. Verifica la tabla conntrack del host:
   ```bash
   sudo conntrack -L 2>/dev/null | head -5
   # o
   cat /proc/net/nf_conntrack 2>/dev/null | head -5
   ```

5. Para una VM que sirve tráfico web con miles de conexiones
   concurrentes, ¿cuál sería más eficiente? ¿Y para una VM de lab que
   solo usa SSH y apt?

**Pregunta de reflexión**: en la práctica, la diferencia de rendimiento
entre NAT y bridge es mínima para la mayoría de cargas de trabajo. ¿En qué
escenario específico el overhead del NAT se volvería significativo?
(Pista: piensa en miles de conexiones concurrentes o tráfico de muy alta
velocidad.)
