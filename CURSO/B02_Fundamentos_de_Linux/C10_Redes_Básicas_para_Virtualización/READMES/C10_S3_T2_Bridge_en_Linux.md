# T02 — Bridge en Linux

## Erratas detectadas en los archivos fuente

| Archivo | Linea | Error | Correccion |
|---------|-------|-------|------------|
| `README.max.md` | 233-234 | `ip link show master eth0` con comentario "eth0 esta en el bridge?". El filtro `master` muestra interfaces cuyo master es el dispositivo indicado. `eth0` no es un bridge, es un puerto — este comando no devuelve nada util. | Para verificar si eth0 esta en un bridge: `ip link show dev eth0` (buscar "master br0" en la salida) o `bridge link show dev eth0`. |

---

## 1. Soporte de bridge en el kernel

Linux implementa el bridge como un **modulo del kernel**. En la mayoria de distribuciones viene compilado o cargado automaticamente:

```bash
# Verificar que el modulo esta cargado
lsmod | grep bridge
# bridge                286720  1 br_netfilter
# stp                    16384  1 bridge
# llc                    16384  2 bridge,stp
```

| Modulo | Funcion |
|--------|---------|
| `bridge` | Implementacion del bridge (MAC learning, forwarding) |
| `stp` | Spanning Tree Protocol (prevencion de loops) |
| `llc` | Logical Link Control (capa 2, requerido por STP) |
| `br_netfilter` | Permite que iptables/nftables inspeccione trafico del bridge |

Si el modulo no esta cargado, se carga automaticamente al crear un bridge:

```bash
# Cargar manualmente (raro que sea necesario)
sudo modprobe bridge
```

### br_netfilter: bridge y firewall

El modulo `br_netfilter` permite que las reglas de iptables/nftables se apliquen al trafico que cruza el bridge. Libvirt lo necesita para sus reglas de seguridad:

```bash
# Verificar
lsmod | grep br_netfilter

# Si no esta cargado y lo necesitas:
sudo modprobe br_netfilter

# Para que se cargue al arrancar:
echo "br_netfilter" | sudo tee /etc/modules-load.d/br_netfilter.conf
```

Parametros controlados por br_netfilter:

```bash
# iptables filtra trafico del bridge?
cat /proc/sys/net/bridge/bridge-nf-call-iptables
# 1 = si (libvirt necesita esto)
# 0 = no (mejor rendimiento si no necesitas filtrar)
```

---

## 2. Herramientas: iproute2 vs bridge-utils

Hay dos conjuntos de herramientas para gestionar bridges:

### iproute2 (moderno, recomendado)

Viene preinstalado en todas las distribuciones modernas. Usa los comandos `ip link` y `bridge`:

```bash
# Crear bridge
ip link add br0 type bridge

# Anadir interfaz
ip link set enp0s3 master br0

# Ver bridges
ip link show type bridge
bridge link show
```

### bridge-utils (clasico, legacy)

Paquete adicional que provee el comando `brctl`:

```bash
# Instalar
sudo dnf install bridge-utils    # Fedora/RHEL
sudo apt install bridge-utils    # Debian/Ubuntu

# Crear bridge
brctl addbr br0

# Anadir interfaz
brctl addif br0 enp0s3

# Ver bridges
brctl show
```

### Equivalencias

| Tarea | iproute2 (moderno) | brctl (legacy) |
|-------|-------------------|----------------|
| Crear bridge | `ip link add br0 type bridge` | `brctl addbr br0` |
| Eliminar bridge | `ip link del br0` | `brctl delbr br0` |
| Anadir interfaz | `ip link set eth0 master br0` | `brctl addif br0 eth0` |
| Quitar interfaz | `ip link set eth0 nomaster` | `brctl delif br0 eth0` |
| Activar bridge | `ip link set br0 up` | `ip link set br0 up` |
| Listar bridges | `ip link show type bridge` | `brctl show` |
| Ver puertos | `bridge link show` | `brctl show` |
| Ver MACs | `bridge fdb show br br0` | `brctl showmacs br0` |
| Ver STP | `bridge stp show` | `brctl showstp br0` |
| Activar STP | `ip link set br0 type bridge stp_state 1` | `brctl stp br0 on` |

En este curso usamos **iproute2** por ser el estandar actual. `brctl` aparece solo como referencia porque lo encontraras en documentacion antigua y en sistemas legacy.

---

## 3. Crear un bridge con iproute2 (temporal)

Los bridges creados con `ip link` son **temporales** — desaparecen al reiniciar. Son utiles para experimentar y aprender.

### Escenario: bridge de prueba sin tocar la red del host

```bash
# Crear un bridge aislado (sin anadir la interfaz fisica)
sudo ip link add br-test type bridge
sudo ip link set br-test up

# Verificar que existe
ip link show type bridge
# br-test: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500

# Asignar una IP al bridge (red privada para pruebas)
sudo ip addr add 10.99.0.1/24 dev br-test

# Verificar
ip addr show br-test
# inet 10.99.0.1/24 scope global br-test

# Limpiar cuando termines
sudo ip link del br-test
```

Este bridge no tiene ninguna interfaz conectada — es solo un bridge vacio con una IP. Cuando conectes VMs a el, podran comunicarse entre si y con el host (via 10.99.0.1), pero no con la LAN ni internet (no hay NAT ni uplink).

### Escenario: bridge conectado a la interfaz fisica

**ADVERTENCIA**: esto cambia la configuracion de red del host. Si estas conectado por SSH, puedes perder la conexion. Hazlo en una consola local o en una VM de prueba.

```bash
# Paso 1: Crear el bridge
sudo ip link add br0 type bridge
sudo ip link set br0 up

# Paso 2: Guardar la configuracion actual de la interfaz
ip addr show enp0s3
# inet 192.168.1.50/24 brd 192.168.1.255 scope global dynamic enp0s3
ip route show default
# default via 192.168.1.1 dev enp0s3

# Paso 3: Mover la interfaz al bridge y transferir la IP
# HACER TODO EN UNA SOLA LINEA para minimizar la desconexion:
sudo ip addr flush dev enp0s3 && \
sudo ip link set enp0s3 master br0 && \
sudo ip addr add 192.168.1.50/24 dev br0 && \
sudo ip route add default via 192.168.1.1 dev br0

# Paso 4: Verificar
ip addr show br0
# inet 192.168.1.50/24 scope global br0

ip addr show enp0s3
# (sin IP -- es un puerto del bridge)

bridge link show
# enp0s3: <BROADCAST,MULTICAST,UP,LOWER_UP> master br0

ping 192.168.1.1
# Deberia funcionar -- la red sigue operativa
```

### Diagrama de lo que paso

```
ANTES:
  enp0s3 (192.168.1.50/24) ---- router (192.168.1.1)

DESPUES:
  br0 (192.168.1.50/24)
   |
   +-- enp0s3 (sin IP, puerto del bridge) ---- router (192.168.1.1)
   |
   +-- (aqui conectarias vnetN de las VMs)
```

### Deshacer el bridge temporal

```bash
# Quitar la interfaz del bridge
sudo ip link set enp0s3 nomaster

# Eliminar el bridge
sudo ip link del br0

# Reasignar la IP a la interfaz fisica
sudo ip addr add 192.168.1.50/24 dev enp0s3
sudo ip route add default via 192.168.1.1 dev enp0s3

# O simplemente: reiniciar NetworkManager
sudo systemctl restart NetworkManager
```

---

## 4. Inspeccionar un bridge

### Ver bridges existentes

```bash
# Listar todos los bridges
ip link show type bridge
# 3: virbr0: <NO-CARRIER,BROADCAST,MULTICAST,UP> mtu 1500 state DOWN
# 7: br0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 state UP

# Vista detallada
ip -d link show br0
# br0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500
#     bridge forward_delay 1500 hello_time 200 max_age 2000
#     ageing_time 30000 stp_state 1 priority 32768
```

### Ver interfaces miembro del bridge

```bash
# Que interfaces estan conectadas al bridge
bridge link show
# 2: enp0s3: <BROADCAST,MULTICAST,UP,LOWER_UP> master br0 state forwarding
# 8: vnet0: <BROADCAST,MULTICAST,UP,LOWER_UP> master br0 state forwarding
# 9: vnet1: <BROADCAST,MULTICAST,UP,LOWER_UP> master br0 state forwarding

# O filtrar por bridge
bridge link show br0

# Alternativa
ip link show master br0

# Verificar si una interfaz especifica esta en un bridge
ip link show dev enp0s3
# Buscar "master br0" en la salida
```

### Estados de los puertos

| Estado | Significado |
|--------|------------|
| `disabled` | Puerto administrativamente deshabilitado |
| `listening` | STP: escuchando BPDUs (no reenvia aun) |
| `learning` | STP: aprendiendo MACs (no reenvia aun) |
| `forwarding` | Activo: reenviando tramas normalmente |
| `blocking` | STP: bloqueado para evitar loops |

En un lab simple sin STP activo o sin loops, todos los puertos estaran en `forwarding`.

### Estadisticas del bridge

```bash
# Trafico por interfaz del bridge
ip -s link show br0
# RX: bytes  packets  errors  dropped
# TX: bytes  packets  errors  dropped

# Estadisticas detalladas
bridge -s link show
```

---

## 5. La tabla FDB (Forwarding Database)

La FDB (Forwarding Database) es la tabla MAC del bridge — mapea MACs a puertos:

```bash
# Ver la tabla FDB completa
bridge fdb show br br0
# 52:54:00:aa:11:11 dev vnet0 master br0
# 52:54:00:aa:22:22 dev vnet1 master br0
# 08:00:27:bb:33:33 dev enp0s3 master br0
# 33:33:00:00:00:01 dev br0 self permanent    <- multicast, permanente

# Con brctl (formato diferente, misma info)
brctl showmacs br0
# port no   mac addr            is local?   ageing timer
#   1       08:00:27:bb:33:33   no           12.34
#   2       52:54:00:aa:11:11   no           45.67
#   3       52:54:00:aa:22:22   no            8.90
```

### Tipos de entradas FDB

| Tipo | Como se crea | Cuando expira |
|------|-------------|---------------|
| `dynamic` (learned) | Automaticamente al ver trafico | Despues del aging time (300s) |
| `permanent` | Manualmente o por el kernel | Nunca (hasta que se borra) |
| `self` | Pertenece al bridge mismo | Permanente |

### Anadir una entrada estatica (raro, pero posible)

```bash
# Forzar que una MAC siempre vaya por un puerto especifico
sudo bridge fdb add 52:54:00:ff:ff:ff dev vnet0 master br0 permanent
```

### Limpiar la tabla FDB

```bash
# Borrar todas las entradas dinamicas (aprendidas)
sudo bridge fdb flush dev br0
```

---

## 6. Crear un bridge permanente con NetworkManager

NetworkManager es el gestor de red en Fedora, RHEL, Ubuntu Desktop, y la mayoria de distribuciones modernas. Un bridge creado con `nmcli` **sobrevive reinicios**.

### Paso a paso con nmcli

```bash
# Paso 1: Crear la conexion del bridge
sudo nmcli connection add type bridge \
    ifname br0 \
    con-name br0 \
    bridge.stp no

# Paso 2: Configurar IP (DHCP)
sudo nmcli connection modify br0 ipv4.method auto

# O IP estatica:
sudo nmcli connection modify br0 \
    ipv4.method manual \
    ipv4.addresses 192.168.1.50/24 \
    ipv4.gateway 192.168.1.1 \
    ipv4.dns "8.8.8.8 1.1.1.1"

# Paso 3: Anadir la interfaz fisica como puerto del bridge
sudo nmcli connection add type bridge-slave \
    ifname enp0s3 \
    master br0 \
    con-name br0-port-enp0s3

# Paso 4: Activar el bridge
sudo nmcli connection up br0

# Paso 5: Verificar
nmcli connection show
# NAME              TYPE        DEVICE
# br0               bridge      br0
# br0-port-enp0s3   bridge-slave enp0s3

ip addr show br0
bridge link show
```

### Desglose de los comandos

| Opcion | Significado |
|--------|------------|
| `type bridge` | Crear una conexion de tipo bridge |
| `ifname br0` | Nombre de la interfaz del bridge |
| `con-name br0` | Nombre de la conexion en NetworkManager |
| `bridge.stp no` | Deshabilitar STP (para labs simples) |
| `type bridge-slave` | Crear un puerto del bridge |
| `master br0` | Este puerto pertenece al bridge br0 |

### Verificar la configuracion persistente

```bash
# Los archivos de configuracion se guardan en:
ls /etc/NetworkManager/system-connections/
# br0.nmconnection
# br0-port-enp0s3.nmconnection

# Ver el contenido
sudo cat /etc/NetworkManager/system-connections/br0.nmconnection
# [connection]
# id=br0
# type=bridge
# interface-name=br0
#
# [bridge]
# stp=false
#
# [ipv4]
# method=auto
```

### Eliminar el bridge

```bash
sudo nmcli connection delete br0-port-enp0s3
sudo nmcli connection delete br0

# Reactivar la conexion original de enp0s3 (si existia):
sudo nmcli connection up "Wired connection 1"
```

---

## 7. Crear un bridge permanente con systemd-networkd

En servidores minimalistas (Fedora CoreOS, Debian server sin NetworkManager, etc.), systemd-networkd gestiona la red.

### Archivos de configuracion

Crear tres archivos en `/etc/systemd/network/`:

```bash
# 1. Definir el bridge
sudo tee /etc/systemd/network/10-br0.netdev << 'EOF'
[NetDev]
Name=br0
Kind=bridge

[Bridge]
STP=false
EOF
```

```bash
# 2. Configurar la IP del bridge
sudo tee /etc/systemd/network/20-br0.network << 'EOF'
[Match]
Name=br0

[Network]
DHCP=yes
# O estatica:
# Address=192.168.1.50/24
# Gateway=192.168.1.1
# DNS=8.8.8.8
EOF
```

```bash
# 3. Anadir la interfaz fisica al bridge
sudo tee /etc/systemd/network/30-enp0s3.network << 'EOF'
[Match]
Name=enp0s3

[Network]
Bridge=br0
EOF
```

```bash
# Aplicar la configuracion
sudo systemctl restart systemd-networkd

# Verificar
networkctl status br0
bridge link show
```

---

## 8. Crear un bridge permanente con netplan (Ubuntu)

Ubuntu Server usa netplan como capa de abstraccion sobre NetworkManager o systemd-networkd:

```bash
sudo tee /etc/netplan/01-bridge.yaml << 'EOF'
network:
  version: 2
  renderer: networkd

  ethernets:
    enp0s3:
      dhcp4: false

  bridges:
    br0:
      interfaces:
        - enp0s3
      dhcp4: true
      # O estatica:
      # addresses:
      #   - 192.168.1.50/24
      # routes:
      #   - to: default
      #     via: 192.168.1.1
      # nameservers:
      #   addresses: [8.8.8.8, 1.1.1.1]
      parameters:
        stp: false
        forward-delay: 0
EOF

# Validar
sudo netplan try    # aplica temporalmente, revierte en 120s si no confirmas

# Aplicar permanentemente
sudo netplan apply
```

---

## 9. Conectar VMs de libvirt a un bridge externo

Una vez que tienes un bridge `br0` funcional, puedes conectar VMs de libvirt a el.

### Metodo 1: definir una red libvirt tipo bridge

```bash
cat > /tmp/bridge-net.xml << 'EOF'
<network>
  <name>host-bridge</name>
  <forward mode='bridge'/>
  <bridge name='br0'/>
</network>
EOF

sudo virsh net-define /tmp/bridge-net.xml
sudo virsh net-start host-bridge
sudo virsh net-autostart host-bridge

# Verificar
virsh net-list --all
# Name          State    Autostart   Persistent
# default       active   yes         yes
# host-bridge   active   yes         yes
```

Ahora al crear una VM puedes especificar `--network network=host-bridge`:

```bash
virt-install \
    --name web-server \
    --network network=host-bridge \
    ...
```

### Metodo 2: conectar directamente al bridge (sin red libvirt)

```bash
virt-install \
    --name web-server \
    --network bridge=br0 \
    ...
```

O modificar una VM existente:

```bash
virsh edit web-server
```

Cambiar:
```xml
<!-- De NAT: -->
<interface type='network'>
  <source network='default'/>
  <model type='virtio'/>
</interface>

<!-- A bridge: -->
<interface type='bridge'>
  <source bridge='br0'/>
  <model type='virtio'/>
</interface>
```

### Que cambia para la VM

| Aspecto | Con NAT (default) | Con bridge (br0) |
|---------|-------------------|-----------------|
| IP de la VM | 192.168.122.x (DHCP de dnsmasq) | 192.168.1.x (DHCP del router real) |
| Gateway | 192.168.122.1 (virbr0) | 192.168.1.1 (router real) |
| DNS | 192.168.122.1 (dnsmasq) | DNS del router real |
| Acceso desde la LAN | No (port forwarding) | Si (IP en la misma red) |
| DHCP server | dnsmasq (libvirt) | Router domestico |

### Permitir que qemu-bridge-helper acceda al bridge

Si usas QEMU sin libvirt, necesitas autorizar el bridge:

```bash
# /etc/qemu/bridge.conf (crear si no existe)
echo "allow br0" | sudo tee -a /etc/qemu/bridge.conf

# Verificar que qemu-bridge-helper tiene setuid
ls -la /usr/lib/qemu/qemu-bridge-helper
# -rwsr-xr-x 1 root root  <- el bit 's' es necesario
```

---

## 10. Configuracion avanzada del bridge

### Parametros del bridge

```bash
# Ver todos los parametros
ip -d link show br0

# Parametros accesibles via sysfs
ls /sys/class/net/br0/bridge/
# ageing_time        forward_delay      max_age
# bridge_id          hello_time         priority
# stp_state          ...
```

| Parametro | Default | Significado |
|-----------|---------|------------|
| `ageing_time` | 30000 (300s) | Tiempo antes de olvidar una MAC inactiva |
| `stp_state` | 1 (on) | Spanning Tree Protocol activo |
| `forward_delay` | 1500 (15s) | Tiempo de espera STP antes de reenviar |
| `hello_time` | 200 (2s) | Intervalo de mensajes hello de STP |
| `max_age` | 2000 (20s) | Tiempo maximo para considerar un BPDU valido |
| `priority` | 32768 | Prioridad del bridge para eleccion de root bridge |

Los valores en sysfs estan en centesimas de segundo (jiffies).

### Modificar parametros

```bash
# Desactivar STP (para labs simples)
sudo ip link set br0 type bridge stp_state 0

# Reducir forward_delay (arranque mas rapido)
sudo ip link set br0 type bridge forward_delay 0

# Cambiar aging time
sudo ip link set br0 type bridge ageing_time 12000    # 120 segundos
```

### Filtrado de VLANs (avanzado)

Linux bridge soporta VLAN filtering para segmentar trafico:

```bash
# Habilitar VLAN filtering
sudo ip link set br0 type bridge vlan_filtering 1

# Asignar VLAN 100 a un puerto
sudo bridge vlan add vid 100 dev vnet0

# Ver VLANs por puerto
bridge vlan show
```

Para este curso no usaremos VLANs en bridges, pero es util saber que la capacidad existe.

---

## 11. El bridge virbr0 de libvirt vs un bridge manual

Es comun confundir virbr0 (el bridge de la red NAT de libvirt) con un bridge manual conectado a la red fisica. Son conceptos diferentes:

### virbr0 (bridge NAT de libvirt)

```
VM (.122.100) -> vnet0 -> virbr0 -> [NAT/MASQUERADE] -> enp0s3 -> router
                         |
                         | IP: 192.168.122.1
                         | Funcion: bridge + gateway + DHCP + DNS + NAT
```

- Creado y gestionado por libvirt automaticamente.
- **Tiene NAT encima**: virbr0 es un bridge, pero el trafico que sale de el pasa por iptables MASQUERADE.
- **Tiene su propia red**: 192.168.122.0/24 (diferente a la LAN).
- **Incluye servicios**: DHCP (dnsmasq), DNS (dnsmasq), NAT (iptables).
- **No** esta conectado a enp0s3 como puerto del bridge.

### br0 (bridge manual conectado a la interfaz fisica)

```
VM (.1.150) -> vnet0 -> br0 -> enp0s3 -> router
                       |
                       | IP: 192.168.1.50 (la IP del host, movida aqui)
                       | Funcion: solo bridge (switch virtual)
```

- Creado por ti manualmente (o con NetworkManager).
- **No tiene NAT**: el trafico pasa directo a la red fisica.
- **Misma red que la LAN**: las VMs obtienen IPs del DHCP del router real.
- **No incluye servicios**: no tiene DHCP ni DNS propios.
- **enp0s3 es un puerto** del bridge (la interfaz fisica pierde su IP).

### Tabla comparativa

| Aspecto | virbr0 (libvirt NAT) | br0 (bridge manual) |
|---------|---------------------|-------------------|
| Quien lo crea | libvirt automaticamente | Tu (nmcli, ip link, etc.) |
| Red | 192.168.122.0/24 (privada) | La red del host (192.168.1.0/24) |
| NAT | Si (MASQUERADE) | No |
| DHCP | dnsmasq (192.168.122.2-254) | Router real de la LAN |
| DNS | dnsmasq (forwarding) | DNS del router/ISP |
| Interfaz fisica | No conectada | enp0s3 es puerto del bridge |
| VM visible en LAN | No | Si |
| Gestion | `virsh net-*` | `nmcli`, `ip link`, archivos de config |

---

## 12. Errores comunes

### Error 1: Crear un bridge por SSH y perder la conexion

```bash
# Estas conectado por SSH a 192.168.1.50 (enp0s3)
sudo ip link set enp0s3 master br0
# -> enp0s3 pierde su IP
# -> SSH se congela
# -> No puedes arreglar nada remotamente
```

**Prevencion:**

```bash
# Opcion A: hacer todo en un solo comando
sudo ip addr flush dev enp0s3 && \
sudo ip link set enp0s3 master br0 && \
sudo ip addr add 192.168.1.50/24 dev br0 && \
sudo ip route add default via 192.168.1.1 dev br0

# Opcion B: usar nmcli (gestiona la transicion por ti)
sudo nmcli connection add type bridge ifname br0 con-name br0
sudo nmcli connection add type bridge-slave ifname enp0s3 master br0
sudo nmcli connection up br0

# Opcion C: hacerlo desde la consola local (no SSH)
```

### Error 2: El bridge existe pero no tiene IP

```bash
ip addr show br0
# NO hay linea "inet ..."

ping 192.168.1.1
# connect: Network is unreachable
```

El bridge necesita una IP para que el host tenga conectividad:

```bash
# DHCP:
sudo dhclient br0

# Estatica:
sudo ip addr add 192.168.1.50/24 dev br0
sudo ip route add default via 192.168.1.1 dev br0
```

### Error 3: NetworkManager reconfigura todo al activarse

```bash
# Creas un bridge con ip link (temporal)
# NetworkManager ve la interfaz "suelta" y la reclama
# Tu configuracion desaparece
```

Soluciones:

```bash
# Opcion A: decirle a NM que no toque la interfaz
sudo nmcli device set enp0s3 managed no

# Opcion B: usar nmcli para crear el bridge (NM lo gestiona correctamente)
# (ver seccion 6)

# Opcion C: desactivar NM completamente (solo para servidores)
sudo systemctl disable --now NetworkManager
```

### Error 4: La VM obtiene IP del rango equivocado

```bash
# Esperabas que la VM obtuviera IP de la LAN (192.168.1.x)
# Pero obtiene 192.168.122.x

# Causa: la VM sigue conectada a la red NAT, no al bridge
virsh domiflist mi-vm
# Si dice "source: default" -> esta en NAT, no en el bridge

# Solucion:
virsh edit mi-vm
# Cambiar source network='default' por source bridge='br0'
```

### Error 5: Olvidar activar el bridge con ip link set up

```bash
sudo ip link add br0 type bridge
sudo ip link set enp0s3 master br0
# El bridge existe pero no funciona...

ip link show br0
# br0: <BROADCAST,MULTICAST> mtu 1500 state DOWN
#                             <- falta UP

sudo ip link set br0 up
# Ahora: <BROADCAST,MULTICAST,UP,LOWER_UP> state UP
```

---

## 13. Cheatsheet

```text
+======================================================================+
|                    BRIDGE EN LINUX CHEATSHEET                          |
+======================================================================+
|                                                                      |
|  CREAR BRIDGE (TEMPORAL -- iproute2)                                 |
|  ip link add br0 type bridge          Crear bridge                   |
|  ip link set br0 up                   Activar bridge                 |
|  ip link set enp0s3 master br0        Anadir interfaz al bridge      |
|  ip addr add 192.168.1.50/24 dev br0  Asignar IP al bridge          |
|  ip route add default via 192.168.1.1 Agregar ruta por defecto       |
|  ip link set enp0s3 nomaster          Quitar interfaz del bridge     |
|  ip link del br0                      Eliminar bridge                |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  CREAR BRIDGE (PERMANENTE -- NetworkManager)                         |
|  nmcli con add type bridge ifname br0 con-name br0                   |
|  nmcli con add type bridge-slave ifname enp0s3 master br0            |
|  nmcli con modify br0 ipv4.method auto    (DHCP)                     |
|  nmcli con up br0                                                    |
|  nmcli con delete br0                     (eliminar)                 |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  INSPECCION                                                          |
|  ip link show type bridge        Listar bridges                      |
|  bridge link show                Ver puertos de cada bridge          |
|  bridge fdb show br br0          Ver tabla MAC (FDB)                 |
|  ip -d link show br0             Parametros del bridge               |
|  ip -s link show br0             Estadisticas de trafico             |
|  bridge link show br0            Estado de puertos                   |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  CONECTAR VMs DE LIBVIRT AL BRIDGE                                   |
|  virsh edit <vm>                                                     |
|    <interface type='bridge'>                                         |
|      <source bridge='br0'/>                                          |
|      <model type='virtio'/>                                          |
|    </interface>                                                      |
|                                                                      |
|  O definir red libvirt tipo bridge:                                  |
|    <forward mode='bridge'/> <bridge name='br0'/>                     |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  virbr0 vs br0                                                       |
|  virbr0:  bridge + NAT + DHCP + DNS (libvirt, red privada)           |
|  br0:     bridge puro (manual, misma red que la LAN)                 |
|                                                                      |
+----------------------------------------------------------------------+
|                                                                      |
|  TROUBLESHOOTING                                                     |
|  Bridge sin IP -> ip addr add / dhclient br0                        |
|  SSH se cuelga -> hacer todo en un solo comando / usar nmcli         |
|  NM interfiere -> nmcli device set <if> managed no                   |
|  VM en red equivocada -> virsh domiflist / virsh edit                |
|  Bridge DOWN -> ip link set br0 up                                   |
|                                                                      |
+======================================================================+
```

---

## 14. Ejercicios

### Ejercicio 1: Bridge temporal de prueba

Crea un bridge aislado sin tocar tu red real:

```bash
sudo ip link add br-lab type bridge
sudo ip link set br-lab up
sudo ip addr add 10.99.0.1/24 dev br-lab
```

**Pregunta:** El bridge tiene interfaces miembro? Si conectaras una VM a este bridge, tendria internet? Que necesitarias para darle internet?

<details><summary>Prediccion</summary>

- **No tiene interfaces miembro**. Es un bridge vacio — no se ha anadido ninguna interfaz con `ip link set <if> master br-lab`. Solo tiene una IP asignada (10.99.0.1/24).

- Una VM conectada a este bridge **no tendria internet**. El bridge no tiene uplink (no esta conectado a enp0s3 ni a ninguna interfaz con salida a internet). La VM podria comunicarse con el host (via 10.99.0.1) y con otras VMs en el mismo bridge, pero no mas alla.

- Para darle internet hay dos opciones:
  1. **Conectar enp0s3 al bridge** (bridge puro): `ip link set enp0s3 master br-lab`. Las VMs obtendrian IPs de la LAN real. Pero habria que mover la IP del host a br-lab.
  2. **Configurar NAT** (como hace libvirt con virbr0): habilitar `ip_forward=1` y anadir una regla de MASQUERADE en iptables para el trafico de 10.99.0.0/24 que sale por enp0s3. Es mas complejo pero no toca la red del host.

Limpiar: `sudo ip link del br-lab`

</details>

---

### Ejercicio 2: Inspeccion de virbr0

Si tienes libvirt instalado, examina virbr0:

```bash
ip -d link show virbr0
ip addr show virbr0
bridge link show virbr0
bridge fdb show br virbr0
cat /sys/class/net/virbr0/bridge/stp_state
cat /sys/class/net/virbr0/bridge/ageing_time
```

**Pregunta:** STP esta activado en virbr0? Cual es el aging time en segundos? virbr0 tiene enp0s3 como puerto? Por que no?

<details><summary>Prediccion</summary>

- **STP**: probablemente **activado** (stp_state = 1). libvirt configura `stp='on'` por defecto en sus bridges, aunque para un bridge con pocas VMs y sin loops, STP no hace nada util.

- **Aging time**: el valor en sysfs sera **30000** (centesimas de segundo), que equivale a **300 segundos** (5 minutos). Es el tiempo estandar antes de olvidar una MAC inactiva.

- **enp0s3 NO es puerto de virbr0**. virbr0 es el bridge de la red NAT de libvirt — no esta conectado a la interfaz fisica. Solo tiene como puertos los TAP devices (vnetN) de las VMs que estan corriendo. Si no hay VMs activas, `bridge link show virbr0` no mostrara ningun puerto.

virbr0 no necesita enp0s3 porque el trafico de las VMs sale a internet via **routing + NAT** (MASQUERADE en iptables), no via bridging a la red fisica. Son mecanismos diferentes: bridge = capa 2, NAT = capa 3.

</details>

---

### Ejercicio 3: iproute2 vs brctl

Escribe los comandos equivalentes con iproute2 para cada operacion de brctl:

```bash
brctl addbr mybridge
brctl addif mybridge eth0
brctl delif mybridge eth0
brctl delbr mybridge
brctl showmacs mybridge
```

<details><summary>Prediccion</summary>

| brctl (legacy) | iproute2 (moderno) |
|----------------|-------------------|
| `brctl addbr mybridge` | `ip link add mybridge type bridge` |
| `brctl addif mybridge eth0` | `ip link set eth0 master mybridge` |
| `brctl delif mybridge eth0` | `ip link set eth0 nomaster` |
| `brctl delbr mybridge` | `ip link del mybridge` |
| `brctl showmacs mybridge` | `bridge fdb show br mybridge` |

Nota: `brctl delbr` requiere que el bridge este down y sin interfaces. `ip link del` es mas directo — elimina el bridge y desvincula automaticamente las interfaces.

</details>

---

### Ejercicio 4: Secuencia critica — bridge por SSH

Estas conectado por SSH a un host con IP 192.168.1.50 en enp0s3. Necesitas crear un bridge br0 con enp0s3 como puerto.

**Pregunta:** Cual es el peligro si ejecutas los comandos uno por uno? Escribe la secuencia segura encadenada con `&&`.

<details><summary>Prediccion</summary>

**Peligro**: si ejecutas `ip link set enp0s3 master br0` por separado, enp0s3 pierde su IP inmediatamente (pasa a ser puerto del bridge). Tu sesion SSH se congela porque la IP 192.168.1.50 ya no existe en enp0s3 y aun no esta en br0. No puedes arreglar nada remotamente.

**Secuencia segura:**

```bash
sudo ip link add br0 type bridge && \
sudo ip link set br0 up && \
sudo ip addr flush dev enp0s3 && \
sudo ip link set enp0s3 master br0 && \
sudo ip addr add 192.168.1.50/24 dev br0 && \
sudo ip route add default via 192.168.1.1 dev br0
```

Todo se ejecuta como una sola operacion atomica. Si cualquier comando falla (por el `&&`), los siguientes no se ejecutan, limitando el dano. La desconexion es momentanea (milisegundos) mientras la IP se mueve de enp0s3 a br0.

Alternativa mas segura: usar `nmcli`, que gestiona la transicion internamente sin perder la conexion.

</details>

---

### Ejercicio 5: FDB — tabla de forwarding

Tu bridge br0 lleva 10 minutos funcionando con 2 VMs y la interfaz fisica. Ejecutas `bridge fdb show br br0` y ves:

```
52:54:00:aa:11:11 dev vnet0 master br0
52:54:00:aa:22:22 dev vnet1 master br0
08:00:27:bb:33:33 dev enp0s3 master br0
33:33:00:00:00:01 dev br0 self permanent
```

**Pregunta:** Que tipo de entrada es cada una? Cual expirara si el dispositivo deja de generar trafico? Por que la entrada `33:33:...` dice `permanent`?

<details><summary>Prediccion</summary>

| Entrada | Tipo | Expira? |
|---------|------|---------|
| `52:54:00:aa:11:11 dev vnet0` | **dynamic** (learned) — aprendida al ver trafico de VM1 | Si, despues del aging time (300s sin trafico) |
| `52:54:00:aa:22:22 dev vnet1` | **dynamic** (learned) — aprendida al ver trafico de VM2 | Si, misma razon |
| `08:00:27:bb:33:33 dev enp0s3` | **dynamic** (learned) — aprendida al ver trafico del dispositivo en la LAN | Si, misma razon |
| `33:33:00:00:00:01 dev br0 self permanent` | **permanent** — creada por el kernel para multicast | No, nunca expira |

La entrada `33:33:00:00:00:01` es una direccion **multicast IPv6** (todas las MACs que empiezan con `33:33` son multicast). El kernel la crea automaticamente (`self permanent`) para que el bridge pueda manejar trafico multicast. No esta asociada a ningun dispositivo fisico — pertenece al bridge mismo.

</details>

---

### Ejercicio 6: Bridge permanente con nmcli

Escribe la secuencia completa de comandos nmcli para crear un bridge br0 con IP estatica 192.168.1.50/24, gateway 192.168.1.1, DNS 8.8.8.8, usando enp0s3 como puerto. Incluye el comando para verificar y el comando para eliminar todo.

<details><summary>Prediccion</summary>

**Crear:**

```bash
# 1. Bridge con STP deshabilitado
sudo nmcli connection add type bridge \
    ifname br0 con-name br0 bridge.stp no

# 2. IP estatica
sudo nmcli connection modify br0 \
    ipv4.method manual \
    ipv4.addresses 192.168.1.50/24 \
    ipv4.gateway 192.168.1.1 \
    ipv4.dns 8.8.8.8

# 3. Anadir enp0s3 como puerto
sudo nmcli connection add type bridge-slave \
    ifname enp0s3 master br0 con-name br0-port-enp0s3

# 4. Activar
sudo nmcli connection up br0
```

**Verificar:**

```bash
nmcli connection show
ip addr show br0
bridge link show
ping 192.168.1.1
```

**Eliminar:**

```bash
sudo nmcli connection delete br0-port-enp0s3
sudo nmcli connection delete br0
sudo nmcli connection up "Wired connection 1"
```

Nota: `nmcli connection up "Wired connection 1"` reactiva la conexion original de enp0s3 para que el host recupere su IP directamente en la interfaz fisica.

</details>

---

### Ejercicio 7: systemd-networkd vs NetworkManager

Para cada sistema, que metodo de creacion de bridge permanente usarias?

1. Fedora Workstation con escritorio GNOME.
2. Debian server sin interfaz grafica, sin NetworkManager.
3. Ubuntu Server 22.04.

<details><summary>Prediccion</summary>

1. **Fedora Workstation**: **nmcli** (NetworkManager). Fedora Desktop y Workstation usan NetworkManager por defecto. Es la forma mas integrada y segura.

2. **Debian server sin NM**: **systemd-networkd**. Sin NetworkManager, la alternativa moderna es systemd-networkd. Se configuran tres archivos en `/etc/systemd/network/`: un `.netdev` para definir el bridge, un `.network` para la IP del bridge, y otro `.network` para anadir enp0s3 al bridge.

3. **Ubuntu Server 22.04**: **netplan**. Ubuntu Server usa netplan como capa de abstraccion. El archivo YAML en `/etc/netplan/` define el bridge, la interfaz y la IP. netplan genera la configuracion para el backend (systemd-networkd o NetworkManager).

Punto clave: la herramienta depende del **gestor de red** que use el sistema, no del tipo de bridge (todos crean el mismo bridge Linux del kernel).

</details>

---

### Ejercicio 8: Conectar VM a bridge externo

Tienes un bridge br0 funcional. Tu VM "web-server" actualmente usa la red NAT (default). Necesitas moverla al bridge.

**Pregunta:** Que cambio harias en el XML de la VM? Despues del cambio, de donde obtendria la VM su IP? Necesitas reiniciar la VM?

<details><summary>Prediccion</summary>

**Cambio en el XML** (via `virsh edit web-server`):

```xml
<!-- ANTES (NAT): -->
<interface type='network'>
  <source network='default'/>
  <model type='virtio'/>
</interface>

<!-- DESPUES (bridge): -->
<interface type='bridge'>
  <source bridge='br0'/>
  <model type='virtio'/>
</interface>
```

**IP de la VM**: la obtendria del **DHCP del router real** de la LAN (192.168.1.1), no de dnsmasq de libvirt. La VM recibiria una IP en el rango de la LAN (ej. 192.168.1.x).

**Reinicio**: **si**, necesitas reiniciar la VM (o al menos desconectar y reconectar la interfaz de red). Los cambios en el XML de libvirt con `virsh edit` se aplican al proximo arranque de la VM. Con la VM corriendo, los cambios no toman efecto hasta:
- `virsh destroy web-server && virsh start web-server` (reinicio frio), o
- `virsh detach-interface` + `virsh attach-interface` (cambio en caliente, mas complejo).

</details>

---

### Ejercicio 9: virbr0 vs br0 — diferencias

Completa la tabla:

| Pregunta | virbr0 | br0 manual |
|----------|--------|-----------|
| Quien asigna IPs a las VMs? | ? | ? |
| La VM puede ser contactada desde la LAN? | ? | ? |
| enp0s3 es puerto del bridge? | ? | ? |
| Tiene NAT? | ? | ? |

<details><summary>Prediccion</summary>

| Pregunta | virbr0 | br0 manual |
|----------|--------|-----------|
| Quien asigna IPs a las VMs? | **dnsmasq** de libvirt (192.168.122.2-254) | **DHCP del router real** de la LAN (192.168.1.x) |
| La VM puede ser contactada desde la LAN? | **No** (esta en red privada 192.168.122.x, necesita port forwarding) | **Si** (tiene IP en la misma red que el host y la LAN) |
| enp0s3 es puerto del bridge? | **No** (virbr0 no esta conectado a la interfaz fisica) | **Si** (enp0s3 es un puerto de br0, pierde su IP) |
| Tiene NAT? | **Si** (MASQUERADE en iptables, configurado por libvirt) | **No** (switching puro de capa 2, sin traduccion de IPs) |

Resumen: virbr0 = bridge + router + NAT + DHCP + DNS (paquete completo, red privada). br0 = solo bridge (switch virtual, misma red que la LAN).

</details>

---

### Ejercicio 10: Planificacion de bridge para lab accesible

Tu host tiene enp0s3 (192.168.1.50/24) y virbr0 (192.168.122.1/24). Necesitas 3 VMs accesibles desde la LAN. Tu host tambien debe mantener conectividad.

**Pregunta:** Escribe el plan completo: comandos para crear br0, XML de la VM, y como verificarias que todo funciona. Pueden coexistir VMs en NAT (virbr0) y VMs en bridge (br0) simultaneamente?

<details><summary>Prediccion</summary>

**Plan:**

```bash
# 1. Crear bridge permanente con nmcli
sudo nmcli connection add type bridge ifname br0 con-name br0 bridge.stp no
sudo nmcli connection modify br0 \
    ipv4.method manual \
    ipv4.addresses 192.168.1.50/24 \
    ipv4.gateway 192.168.1.1 \
    ipv4.dns 8.8.8.8
sudo nmcli connection add type bridge-slave ifname enp0s3 master br0 \
    con-name br0-port-enp0s3
sudo nmcli connection up br0
```

**XML de cada VM** (via `virsh edit`):
```xml
<interface type='bridge'>
  <source bridge='br0'/>
  <model type='virtio'/>
</interface>
```

**Verificacion:**
```bash
# En el host:
ip addr show br0               # Verificar IP del host en br0
bridge link show                # Verificar enp0s3 + vnetN como puertos
ping 192.168.1.1                # Verificar conectividad del host

# En cada VM:
ip addr show eth0               # Verificar IP en rango 192.168.1.x
ping 192.168.1.1                # Verificar que alcanza el router
ping 192.168.1.50               # Verificar que alcanza el host

# Desde otro dispositivo de la LAN:
ping <IP-de-la-VM>              # Verificar accesibilidad
```

**Coexistencia**: **si**, VMs en NAT (virbr0) y VMs en bridge (br0) pueden coexistir perfectamente. Son bridges independientes en redes diferentes. virbr0 sigue funcionando con su red 192.168.122.0/24 y NAT. br0 funciona con la LAN 192.168.1.0/24 sin NAT. Puedes tener VMs de desarrollo en NAT (invisibles) y VMs de servicios en bridge (accesibles).

</details>

---

> **Nota**: crear un bridge en Linux es tecnicamente simple — unos pocos comandos. La complejidad real esta en no perder la conectividad del host durante el proceso (especialmente por SSH), en elegir entre configuracion temporal y permanente, y en entender la diferencia entre virbr0 (bridge+NAT de libvirt) y un bridge manual (bridge puro conectado a la interfaz fisica). Para labs donde solo necesitas que las VMs tengan internet, NAT es mas simple y seguro. Para labs donde otros dispositivos necesitan alcanzar las VMs, el bridge es necesario.
