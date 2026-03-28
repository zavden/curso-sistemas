# Red aislada (isolated)

## Índice

1. [Qué es una red aislada](#qué-es-una-red-aislada)
2. [Red aislada vs red NAT](#red-aislada-vs-red-nat)
3. [Crear una red aislada](#crear-una-red-aislada)
4. [Ciclo de vida de una red](#ciclo-de-vida-de-una-red)
5. [Conectar VMs a la red aislada](#conectar-vms-a-la-red-aislada)
6. [Red aislada sin DHCP](#red-aislada-sin-dhcp)
7. [Múltiples redes aisladas](#múltiples-redes-aisladas)
8. [Uso en el curso](#uso-en-el-curso)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## Qué es una red aislada

Una red aislada es una red virtual **sin acceso al exterior**. Las VMs
conectadas a ella pueden comunicarse entre sí, pero no pueden alcanzar
internet ni la red física del host.

```
┌──────────────────────────────────────────────────────────────────┐
│  HOST                                                            │
│                                                                  │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐                         │
│  │  VM 1   │  │  VM 2   │  │  VM 3   │                         │
│  │ 10.0.1.10│  │ 10.0.1.11│  │ 10.0.1.12│                       │
│  └────┬────┘  └────┬────┘  └────┬────┘                         │
│       │            │            │                                │
│  ─────┴────────────┴────────────┴──── Red aislada "lab-net"     │
│                     │                                            │
│              ┌──────┴──────┐                                    │
│              │  virbr-lab  │  Bridge virtual (10.0.1.1)         │
│              │  + dnsmasq  │  Solo DHCP para las VMs            │
│              └─────────────┘                                    │
│                     ╳        ← SIN NAT, SIN FORWARDING          │
│              ┌─────────────┐                                    │
│              │   eth0/wlan │  Interfaz física                   │
│              └──────┬──────┘                                    │
│                     │                                            │
└─────────────────────┼────────────────────────────────────────────┘
                      │
                 ── Internet ── (las VMs NO llegan aquí)
```

La diferencia con la red default es la **ausencia** del elemento
`<forward>` o su modo establecido en `none`. Sin forwarding, no hay NAT
ni enrutamiento hacia el exterior.

---

## Red aislada vs red NAT

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│  Red NAT (default):              Red aislada (isolated):     │
│                                                              │
│  VM → VM:        ✅               VM → VM:        ✅          │
│  VM → Host:      ✅               VM → Host:      ✅ (bridge) │
│  VM → Internet:  ✅ (NAT)         VM → Internet:  ❌          │
│  Host → VM:      ✅               Host → VM:      ✅ (bridge) │
│  Externo → VM:   ❌               Externo → VM:   ❌          │
│                                                              │
│  XML:                            XML:                        │
│  <forward mode='nat'/>           Sin <forward> o             │
│                                  <forward mode='none'/>      │
│                                                              │
│  iptables: MASQUERADE            iptables: DROP/REJECT       │
│                                  (libvirt bloquea tráfico    │
│                                   hacia fuera explícitamente)│
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

> **Nota**: el host **sí** puede comunicarse con las VMs en la red aislada
> porque el bridge tiene una IP en esa subred. Lo que no existe es el
> enrutamiento hacia la interfaz física del host. Las VMs están
> "encerradas" en su propia subred.

---

## Crear una red aislada

### Paso 1: Escribir el XML de definición

```bash
cat > /tmp/isolated-net.xml <<'EOF'
<network>
  <name>isolated</name>
  <bridge name='virbr-iso' stp='on' delay='0'/>
  <ip address='10.0.1.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.0.1.10' end='10.0.1.254'/>
    </dhcp>
  </ip>
</network>
EOF
```

Observa: **no hay** elemento `<forward>`. Sin él, libvirt crea una red
sin enrutamiento ni NAT.

Cada campo:

| Elemento | Valor | Significado |
|----------|-------|-------------|
| `<name>` | `isolated` | Nombre de la red en virsh |
| `<bridge name='virbr-iso'>` | `virbr-iso` | Nombre del bridge (debe ser único) |
| `<ip address='10.0.1.1'>` | `10.0.1.1/24` | IP del host en esta red |
| `<dhcp><range .../>` | `.10` — `.254` | Rango DHCP para las VMs |

### Paso 2: Definir, arrancar y autostart

```bash
# Definir
sudo virsh net-define /tmp/isolated-net.xml
```

```
Network isolated defined from /tmp/isolated-net.xml
```

```bash
# Arrancar
sudo virsh net-start isolated
```

```
Network isolated started
```

```bash
# Autostart
sudo virsh net-autostart isolated
```

```
Network isolated marked as autostarted
```

### Paso 3: Verificar

```bash
virsh net-list --all
```

```
 Name       State    Autostart   Persistent
----------------------------------------------
 default    active   yes         yes
 isolated   active   yes         yes
```

```bash
ip addr show virbr-iso
```

```
9: virbr-iso: <NO-CARRIER,BROADCAST,MULTICAST,UP> mtu 1500
    inet 10.0.1.1/24 brd 10.0.1.255 scope global virbr-iso
```

```bash
# Verificar que NO hay regla NAT para esta red
sudo iptables -t nat -L -n | grep "10.0.1"
# (sin resultado — correcto, no hay NAT)
```

### Alternativa: modo 'none' explícito

Puedes ser explícito con `<forward mode='none'/>`:

```xml
<network>
  <name>isolated</name>
  <forward mode='none'/>
  <bridge name='virbr-iso' stp='on' delay='0'/>
  <ip address='10.0.1.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.0.1.10' end='10.0.1.254'/>
    </dhcp>
  </ip>
</network>
```

Ambas formas (omitir `<forward>` o usar `mode='none'`) producen el mismo
resultado. La forma explícita es más legible.

---

## Ciclo de vida de una red

Las redes virtuales siguen el mismo ciclo que los pools y las VMs:

```bash
# Definir (registrar en libvirt)
virsh net-define red.xml

# Arrancar (activar)
virsh net-start NOMBRE

# Autostart (arrancar con libvirtd)
virsh net-autostart NOMBRE

# Detener (desactivar)
virsh net-destroy NOMBRE

# Eliminar definición
virsh net-undefine NOMBRE
```

Los archivos de definición se guardan en:

```bash
ls /etc/libvirt/qemu/networks/
# default.xml  isolated.xml

ls /etc/libvirt/qemu/networks/autostart/
# default.xml -> ../default.xml
# isolated.xml -> ../isolated.xml
```

### Editar una red existente

```bash
virsh net-edit isolated
```

Abre el XML en tu editor. Al guardar, libvirt valida y aplica. Algunos
cambios requieren reiniciar la red:

```bash
virsh net-destroy isolated
virsh net-start isolated
```

> **Cuidado**: `net-destroy` desconecta todas las VMs de esa red
> inmediatamente. Apaga las VMs primero o reconéctalas después.

---

## Conectar VMs a la red aislada

### Al crear la VM

```bash
sudo virt-install \
    --name vm-isolated \
    --ram 1024 --vcpus 1 --import \
    --disk path=/var/lib/libvirt/images/vm-isolated.qcow2 \
    --os-variant debian12 \
    --network network=isolated \
    --graphics none --noautoconsole
```

### VM con dos interfaces: default + aislada

Una configuración muy útil: la VM tiene acceso a internet (para instalar
paquetes) **y** está conectada a la red aislada (para el lab):

```bash
sudo virt-install \
    --name vm-dual \
    --ram 1024 --vcpus 1 --import \
    --disk path=/var/lib/libvirt/images/vm-dual.qcow2 \
    --os-variant debian12 \
    --network network=default \
    --network network=isolated \
    --graphics none --noautoconsole
```

Dentro de la VM:

```bash
ip addr
```

```
2: enp1s0: ... inet 192.168.122.x/24    ← red default (internet)
3: enp7s0: ... inet 10.0.1.x/24         ← red aislada (lab)
```

### Agregar interfaz a VM existente

```bash
sudo virsh attach-interface debian-lab \
    network isolated \
    --model virtio --persistent
```

### Verificar las conexiones

```bash
# Ver qué VMs están en cada red
virsh net-dhcp-leases isolated
```

```
 MAC address          IP address        Hostname
----------------------------------------------------
 52:54:00:aa:bb:01    10.0.1.10/24     vm-isolated
 52:54:00:aa:bb:02    10.0.1.11/24     vm-dual
```

---

## Red aislada sin DHCP

Para labs de networking donde quieres que los estudiantes configuren la
red manualmente, puedes crear una red sin DHCP:

```xml
<network>
  <name>manual-net</name>
  <bridge name='virbr-man' stp='on' delay='0'/>
  <ip address='10.0.2.1' netmask='255.255.255.0'>
    <!-- Sin bloque <dhcp> -->
  </ip>
</network>
```

Las VMs conectadas a esta red no obtienen IP automáticamente. Deben
configurarla manualmente dentro del guest:

```bash
# Dentro de la VM:
sudo ip addr add 10.0.2.10/24 dev enp1s0
sudo ip link set enp1s0 up
```

### Red sin IP de host (puro L2)

Si no quieres que el host tenga IP en la red (solo un switch virtual
entre VMs):

```xml
<network>
  <name>l2-only</name>
  <bridge name='virbr-l2' stp='on' delay='0'/>
  <!-- Sin bloque <ip> — solo un bridge sin dirección -->
</network>
```

Esto crea un bridge sin IP. Las VMs se comunican entre sí por MAC
address/ARP, pero el host no tiene presencia IP en esa red.

> **Predicción**: si creas una red sin `<ip>`, el host no lanza dnsmasq
> para esa red (no hay DHCP ni DNS). Es un switch L2 puro.

---

## Múltiples redes aisladas

Puedes crear tantas redes aisladas como necesites, cada una con su propia
subred:

```bash
# Red de "oficina"
cat > /tmp/net-office.xml <<'EOF'
<network>
  <name>office</name>
  <bridge name='virbr-off' stp='on' delay='0'/>
  <ip address='10.0.1.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.0.1.10' end='10.0.1.254'/>
    </dhcp>
  </ip>
</network>
EOF

# Red de "servidores"
cat > /tmp/net-servers.xml <<'EOF'
<network>
  <name>servers</name>
  <bridge name='virbr-srv' stp='on' delay='0'/>
  <ip address='10.0.2.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.0.2.10' end='10.0.2.254'/>
    </dhcp>
  </ip>
</network>
EOF

# Definir y arrancar ambas
for net in /tmp/net-office.xml /tmp/net-servers.xml; do
    sudo virsh net-define "$net"
done
sudo virsh net-start office
sudo virsh net-start servers
sudo virsh net-autostart office
sudo virsh net-autostart servers
```

```bash
virsh net-list
```

```
 Name       State    Autostart   Persistent
----------------------------------------------
 default    active   yes         yes
 office     active   yes         yes
 servers    active   yes         yes
```

### VMs en diferentes redes

```
┌───────────────────────────────────────────────────────────┐
│                                                           │
│  Red "office" (10.0.1.0/24)    Red "servers" (10.0.2.0/24)│
│                                                           │
│  ┌────────┐  ┌────────┐       ┌────────┐  ┌────────┐    │
│  │ PC-1   │  │ PC-2   │       │ WEB    │  │ DB     │    │
│  │.1.10   │  │.1.11   │       │.2.10   │  │.2.11   │    │
│  └───┬────┘  └───┬────┘       └───┬────┘  └───┬────┘    │
│      │           │                │           │          │
│  ────┴───────────┴────            ────┴───────────┴────  │
│      virbr-off                        virbr-srv          │
│                                                           │
│  PC-1 puede alcanzar PC-2  ✅                             │
│  WEB puede alcanzar DB     ✅                             │
│  PC-1 puede alcanzar WEB   ❌  (subredes diferentes,     │
│                                 sin router entre ellas)   │
│                                                           │
└───────────────────────────────────────────────────────────┘
```

Las VMs en redes diferentes **no pueden comunicarse** a menos que haya
un router que las conecte. Esto es exactamente lo que necesitamos para
labs de routing en B09.

---

## Uso en el curso

Las redes aisladas son fundamentales para los bloques de redes, servicios
y seguridad:

### B09 — Redes

```
Lab de routing:

  Red "office"          VM Router          Red "servers"
  10.0.1.0/24     ┌─────────────────┐     10.0.2.0/24
                   │  enp1s0: .1.254 │
  PC-1 (.1.10) ───┤                 ├─── WEB (.2.10)
  PC-2 (.1.11) ───┤  enp7s0: .2.254 │    DB  (.2.11)
                   │  ip_forward = 1 │
                   └─────────────────┘

  Sin el router: PC-1 no puede ver WEB
  Con el router: PC-1 → router → WEB ✅
```

Practicar routing, NAT y firewall sin tocar la red real del host.

### B10 — Servicios

```
Lab de DNS:

  Red "lab-dns" (aislada, sin DHCP de libvirt)
  10.0.3.0/24

  ┌─────────────┐     ┌─────────────┐
  │ DNS Server  │     │  Cliente    │
  │ 10.0.3.1    │     │  10.0.3.10  │
  │ bind9/named │     │             │
  └──────┬──────┘     └──────┬──────┘
         │                   │
  ───────┴───────────────────┴──────

  El DNS server controla TODA la resolución de nombres.
  No hay dnsmasq de libvirt interfiriendo.
```

### B11 — Seguridad

```
Lab de firewall:

  Red "external" ─── VM Firewall ─── Red "internal"
  (simula internet)   (iptables)     (red protegida)

  Si cometes un error en iptables, solo afectas a las VMs.
  Tu conexión SSH al host NUNCA se ve comprometida.
```

> **Regla clave**: los labs de red **siempre** se hacen en redes aisladas.
> Nunca experimentes con firewall o routing en la red default ni en la red
> física del host — el riesgo de cortarte tu propia conexión es real.

---

## Errores comunes

### 1. Subred duplicada

```bash
# ❌ Dos redes con la misma subred
# Red 1: 10.0.1.0/24
# Red 2: 10.0.1.0/24  ← conflicto
virsh net-start red2
# error: internal error: Network is already in use by interface virbr-red1

# ✅ Usar subredes distintas
# Red 1: 10.0.1.0/24
# Red 2: 10.0.2.0/24
```

### 2. Nombre de bridge duplicado

```bash
# ❌ Dos redes con el mismo bridge name
# Red 1: <bridge name='virbr1'/>
# Red 2: <bridge name='virbr1'/>  ← conflicto

# ✅ Nombres únicos
# Red 1: <bridge name='virbr-off'/>
# Red 2: <bridge name='virbr-srv'/>
```

### 3. VM aislada que necesita internet

```bash
# ❌ VM solo en red aislada — no puede hacer apt update
virt-install ... --network network=isolated

# Dentro de la VM:
sudo apt update
# Err: Could not resolve 'deb.debian.org'

# ✅ Añadir segunda interfaz con red default
virsh attach-interface vm-isolated network default --model virtio --persistent
# O crear la VM con ambas redes desde el inicio
```

### 4. Confundir net-destroy con net-undefine

```bash
# net-destroy: DESACTIVA la red (VMs pierden conectividad)
virsh net-destroy isolated
# La red deja de funcionar, pero su definición sigue en libvirt

# net-undefine: ELIMINA la definición
virsh net-undefine isolated
# La red desaparece de libvirt permanentemente

# ⚠ net-destroy desconecta VMs inmediatamente — cuidado
```

### 5. Olvidar arrancar la red antes de crear VMs

```bash
# ❌ Red definida pero no arrancada
virsh net-list --all
# isolated   inactive

virt-install ... --network network=isolated
# error: Network 'isolated' is not active

# ✅ Arrancar primero
virsh net-start isolated
```

---

## Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║  RED AISLADA — REFERENCIA RÁPIDA                                   ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  XML MÍNIMO:                                                       ║
║    <network>                                                       ║
║      <name>NOMBRE</name>                                           ║
║      <bridge name='virbr-NOMBRE' stp='on' delay='0'/>             ║
║      <ip address='10.0.X.1' netmask='255.255.255.0'>              ║
║        <dhcp>                                                      ║
║          <range start='10.0.X.10' end='10.0.X.254'/>              ║
║        </dhcp>                                                     ║
║      </ip>                                                         ║
║    </network>                                                      ║
║    (sin <forward> = aislada)                                       ║
║                                                                    ║
║  CREAR Y ACTIVAR:                                                  ║
║    virsh net-define red.xml                                        ║
║    virsh net-start NOMBRE                                          ║
║    virsh net-autostart NOMBRE                                      ║
║                                                                    ║
║  CONECTAR VM:                                                      ║
║    virt-install ... --network network=NOMBRE                       ║
║    virsh attach-interface VM network NOMBRE --model virtio         ║
║                                                                    ║
║  DOBLE RED (internet + aislada):                                   ║
║    --network network=default --network network=NOMBRE              ║
║                                                                    ║
║  CONSULTAR:                                                        ║
║    virsh net-list --all                                             ║
║    virsh net-dumpxml NOMBRE                                        ║
║    virsh net-dhcp-leases NOMBRE                                    ║
║                                                                    ║
║  ELIMINAR:                                                         ║
║    virsh net-destroy NOMBRE    (desactivar)                        ║
║    virsh net-undefine NOMBRE   (eliminar definición)               ║
║                                                                    ║
║  VARIANTES:                                                        ║
║    Sin DHCP: omitir <dhcp>...</dhcp>                               ║
║    Sin IP host: omitir <ip>...</ip> (switch L2 puro)               ║
║    Explícito: <forward mode='none'/>                               ║
║                                                                    ║
║  REGLA: labs de red SIEMPRE en redes aisladas, NUNCA en default   ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Crear y probar una red aislada

**Objetivo**: crear una red aislada, conectar VMs y verificar el aislamiento.

1. Crea el XML de la red:
   ```bash
   cat > /tmp/lab-isolated.xml <<'EOF'
   <network>
     <name>lab-isolated</name>
     <bridge name='virbr-lab' stp='on' delay='0'/>
     <ip address='10.0.50.1' netmask='255.255.255.0'>
       <dhcp>
         <range start='10.0.50.10' end='10.0.50.254'/>
       </dhcp>
     </ip>
   </network>
   EOF
   ```

2. Defínela y arráncala:
   ```bash
   sudo virsh net-define /tmp/lab-isolated.xml
   sudo virsh net-start lab-isolated
   ```

3. Verifica:
   ```bash
   virsh net-list --all
   ip addr show virbr-lab
   sudo iptables -t nat -L -n | grep "10.0.50"
   # ¿Hay regla NAT para 10.0.50? (no debería haber)
   ```

4. Crea dos VMs, cada una con **dos interfaces** (default + aislada):
   ```bash
   for vm in iso-a iso-b; do
       sudo qemu-img create -f qcow2 \
           -b /var/lib/libvirt/images/tpl-debian12.qcow2 -F qcow2 \
           /var/lib/libvirt/images/${vm}.qcow2
       sudo virt-install --name $vm --ram 512 --vcpus 1 --import \
           --disk path=/var/lib/libvirt/images/${vm}.qcow2 \
           --os-variant debian12 \
           --network network=default \
           --network network=lab-isolated \
           --graphics none --noautoconsole
   done
   sleep 30
   ```

5. Desde iso-a, verifica la conectividad:
   ```bash
   virsh console iso-a
   # Ver las dos interfaces:
   ip addr
   # enp1s0: 192.168.122.x (default)
   # enp7s0: 10.0.50.x    (aislada)

   # Probar internet via default:
   ping -c 2 8.8.8.8        # ✅ debería funcionar

   # Probar comunicación con iso-b via red aislada:
   ping -c 2 10.0.50.11     # ✅ si iso-b tiene esa IP

   # Probar que desde la red aislada NO hay internet:
   # (complicado verificar directamente — lo veremos con VMs solo-aisladas)
   ```

6. Limpia:
   ```bash
   for vm in iso-a iso-b; do
       sudo virsh destroy $vm
       sudo virsh undefine $vm --remove-all-storage
   done
   sudo virsh net-destroy lab-isolated
   sudo virsh net-undefine lab-isolated
   ```

**Pregunta de reflexión**: las VMs tienen dos interfaces: una en default
y otra en la aislada. Si una VM ruta tráfico entre sus interfaces
(`ip_forward=1`), ¿podría la otra VM usar esa ruta para acceder a internet
vía la red aislada? ¿Es esto un riesgo de seguridad en labs?

---

### Ejercicio 2: Red aislada sin DHCP

**Objetivo**: crear una red donde las VMs deben configurar sus IPs
manualmente.

1. Crea una red sin bloque `<dhcp>`:
   ```bash
   cat > /tmp/manual-net.xml <<'EOF'
   <network>
     <name>manual-net</name>
     <bridge name='virbr-man' stp='on' delay='0'/>
     <ip address='172.16.0.1' netmask='255.255.255.0'>
     </ip>
   </network>
   EOF

   sudo virsh net-define /tmp/manual-net.xml
   sudo virsh net-start manual-net
   ```

2. Crea una VM con solo esta red:
   ```bash
   sudo qemu-img create -f qcow2 \
       -b /var/lib/libvirt/images/tpl-debian12.qcow2 -F qcow2 \
       /var/lib/libvirt/images/vm-manual.qcow2
   sudo virt-install --name vm-manual --ram 512 --vcpus 1 --import \
       --disk path=/var/lib/libvirt/images/vm-manual.qcow2 \
       --os-variant debian12 \
       --network network=manual-net \
       --graphics none --noautoconsole
   sleep 20
   ```

3. Conéctate y verifica que no tiene IP:
   ```bash
   virsh console vm-manual
   ip addr show enp1s0
   # Sin inet — no hay DHCP
   ```

4. Configura la IP manualmente:
   ```bash
   sudo ip addr add 172.16.0.10/24 dev enp1s0
   sudo ip link set enp1s0 up
   ip addr show enp1s0
   ```

5. Verifica conectividad con el host (bridge):
   ```bash
   ping -c 2 172.16.0.1
   # ✅ El host está en 172.16.0.1
   ```

6. Verifica que no hay internet:
   ```bash
   ping -c 2 8.8.8.8
   # ❌ Network is unreachable (no hay gateway hacia fuera)
   ```

7. Limpia:
   ```bash
   sudo virsh destroy vm-manual
   sudo virsh undefine vm-manual --remove-all-storage
   sudo virsh net-destroy manual-net
   sudo virsh net-undefine manual-net
   ```

**Pregunta de reflexión**: en un lab de DHCP (B10), ¿por qué es
imprescindible usar una red sin DHCP de libvirt? ¿Qué pasaría si
configuras tu propio servidor DHCP en una red donde dnsmasq ya está
corriendo?

---

### Ejercicio 3: Simular aislamiento total

**Objetivo**: demostrar que la red aislada realmente impide el acceso a
internet.

1. Crea una red aislada con DHCP:
   ```bash
   cat > /tmp/jail-net.xml <<'EOF'
   <network>
     <name>jail</name>
     <bridge name='virbr-jail' stp='on' delay='0'/>
     <ip address='10.99.0.1' netmask='255.255.255.0'>
       <dhcp>
         <range start='10.99.0.10' end='10.99.0.254'/>
       </dhcp>
     </ip>
   </network>
   EOF
   sudo virsh net-define /tmp/jail-net.xml
   sudo virsh net-start jail
   ```

2. Crea una VM con **solo** la red aislada (sin default):
   ```bash
   sudo qemu-img create -f qcow2 \
       -b /var/lib/libvirt/images/tpl-debian12.qcow2 -F qcow2 \
       /var/lib/libvirt/images/vm-jail.qcow2
   sudo virt-install --name vm-jail --ram 512 --vcpus 1 --import \
       --disk path=/var/lib/libvirt/images/vm-jail.qcow2 \
       --os-variant debian12 \
       --network network=jail \
       --graphics none --noautoconsole
   sleep 20
   ```

3. Conéctate y verifica:
   ```bash
   virsh console vm-jail
   ip addr
   # Solo interfaz en 10.99.0.x

   # Gateway (host):
   ping -c 2 10.99.0.1
   # ✅

   # Internet:
   ping -c 2 8.8.8.8
   # ❌ Network is unreachable

   # DNS:
   ping -c 2 google.com
   # ❌ Name resolution failed (o unreachable)
   ```

4. Verifica en el host que no hay NAT:
   ```bash
   sudo iptables -t nat -L -n | grep "10.99"
   # (sin resultado)

   # Pero sí hay reglas de REJECT/DROP para bloquear forwarding:
   sudo iptables -L FORWARD -n | grep "10.99"
   ```

5. Limpia:
   ```bash
   sudo virsh destroy vm-jail
   sudo virsh undefine vm-jail --remove-all-storage
   sudo virsh net-destroy jail
   sudo virsh net-undefine jail
   ```

**Pregunta de reflexión**: la VM puede alcanzar al host (10.99.0.1) porque
el bridge tiene IP en esa subred. ¿Significa esto que la VM podría acceder
a servicios que corren en el host (como un servidor web en puerto 80)?
¿Cómo podrías prevenir eso si fuera un lab de seguridad?
