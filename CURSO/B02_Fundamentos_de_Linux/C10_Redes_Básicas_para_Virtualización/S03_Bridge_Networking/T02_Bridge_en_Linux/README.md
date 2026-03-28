# Bridge en Linux

## Índice

1. [Soporte de bridge en el kernel](#1-soporte-de-bridge-en-el-kernel)
2. [Herramientas: iproute2 vs bridge-utils](#2-herramientas-iproute2-vs-bridge-utils)
3. [Crear un bridge con iproute2 (temporal)](#3-crear-un-bridge-con-iproute2-temporal)
4. [Inspeccionar un bridge](#4-inspeccionar-un-bridge)
5. [La tabla FDB (Forwarding Database)](#5-la-tabla-fdb-forwarding-database)
6. [Crear un bridge permanente con NetworkManager](#6-crear-un-bridge-permanente-con-networkmanager)
7. [Crear un bridge permanente con systemd-networkd](#7-crear-un-bridge-permanente-con-systemd-networkd)
8. [Crear un bridge permanente con netplan (Ubuntu)](#8-crear-un-bridge-permanente-con-netplan-ubuntu)
9. [Conectar VMs de libvirt a un bridge externo](#9-conectar-vms-de-libvirt-a-un-bridge-externo)
10. [Configuración avanzada del bridge](#10-configuración-avanzada-del-bridge)
11. [El bridge virbr0 de libvirt vs un bridge manual](#11-el-bridge-virbr0-de-libvirt-vs-un-bridge-manual)
12. [Errores comunes](#12-errores-comunes)
13. [Cheatsheet](#13-cheatsheet)
14. [Ejercicios](#14-ejercicios)

---

## 1. Soporte de bridge en el kernel

Linux implementa el bridge como un **módulo del kernel**. En la mayoría de distribuciones viene compilado o cargado automáticamente:

```bash
# Verificar que el módulo está cargado
lsmod | grep bridge
# bridge                286720  1 br_netfilter
# stp                    16384  1 bridge
# llc                    16384  2 bridge,stp
```

| Módulo | Función |
|--------|---------|
| `bridge` | Implementación del bridge (MAC learning, forwarding) |
| `stp` | Spanning Tree Protocol (prevención de loops) |
| `llc` | Logical Link Control (capa 2, requerido por STP) |
| `br_netfilter` | Permite que iptables/nftables inspeccione tráfico del bridge |

Si el módulo no está cargado, se carga automáticamente al crear un bridge:

```bash
# Cargar manualmente (raro que sea necesario)
sudo modprobe bridge
```

### br_netfilter: bridge y firewall

El módulo `br_netfilter` permite que las reglas de iptables/nftables se apliquen al tráfico que cruza el bridge. Libvirt lo necesita para sus reglas de seguridad:

```bash
# Verificar
lsmod | grep br_netfilter

# Si no está cargado y lo necesitas:
sudo modprobe br_netfilter

# Para que se cargue al arrancar:
echo "br_netfilter" | sudo tee /etc/modules-load.d/br_netfilter.conf
```

Parámetros controlados por br_netfilter:

```bash
# ¿iptables filtra tráfico del bridge?
cat /proc/sys/net/bridge/bridge-nf-call-iptables
# 1 = sí (libvirt necesita esto)
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

# Añadir interfaz
ip link set enp0s3 master br0

# Ver bridges
ip link show type bridge
bridge link show
```

### bridge-utils (clásico, legacy)

Paquete adicional que provee el comando `brctl`:

```bash
# Instalar
sudo dnf install bridge-utils    # Fedora/RHEL
sudo apt install bridge-utils    # Debian/Ubuntu

# Crear bridge
brctl addbr br0

# Añadir interfaz
brctl addif br0 enp0s3

# Ver bridges
brctl show
```

### Equivalencias

| Tarea | iproute2 (moderno) | brctl (legacy) |
|-------|-------------------|----------------|
| Crear bridge | `ip link add br0 type bridge` | `brctl addbr br0` |
| Eliminar bridge | `ip link del br0` | `brctl delbr br0` |
| Añadir interfaz | `ip link set eth0 master br0` | `brctl addif br0 eth0` |
| Quitar interfaz | `ip link set eth0 nomaster` | `brctl delif br0 eth0` |
| Activar bridge | `ip link set br0 up` | `ip link set br0 up` |
| Listar bridges | `ip link show type bridge` | `brctl show` |
| Ver puertos | `bridge link show` | `brctl show` |
| Ver MACs | `bridge fdb show br br0` | `brctl showmacs br0` |
| Ver STP | `bridge stp show` | `brctl showstp br0` |
| Activar STP | `ip link set br0 type bridge stp_state 1` | `brctl stp br0 on` |

En este curso usamos **iproute2** por ser el estándar actual. `brctl` aparece solo como referencia porque lo encontrarás en documentación antigua y en sistemas legacy.

---

## 3. Crear un bridge con iproute2 (temporal)

Los bridges creados con `ip link` son **temporales** — desaparecen al reiniciar. Son útiles para experimentar y aprender.

### Escenario: bridge de prueba sin tocar la red del host

```bash
# Crear un bridge aislado (sin añadir la interfaz física)
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

Este bridge no tiene ninguna interfaz conectada — es solo un bridge vacío con una IP. Cuando conectes VMs a él, podrán comunicarse entre sí y con el host (via 10.99.0.1), pero no con la LAN ni internet (no hay NAT ni uplink).

### Escenario: bridge conectado a la interfaz física

**ADVERTENCIA**: esto cambia la configuración de red del host. Si estás conectado por SSH, puedes perder la conexión. Hazlo en una consola local o en una VM de prueba.

```bash
# Paso 1: Crear el bridge
sudo ip link add br0 type bridge
sudo ip link set br0 up

# Paso 2: Guardar la configuración actual de la interfaz
ip addr show enp0s3
# inet 192.168.1.50/24 brd 192.168.1.255 scope global dynamic enp0s3
ip route show default
# default via 192.168.1.1 dev enp0s3

# Paso 3: Mover la interfaz al bridge y transferir la IP
# HACER TODO EN UNA SOLA LÍNEA para minimizar la desconexión:
sudo ip addr flush dev enp0s3 && \
sudo ip link set enp0s3 master br0 && \
sudo ip addr add 192.168.1.50/24 dev br0 && \
sudo ip route add default via 192.168.1.1 dev br0

# Paso 4: Verificar
ip addr show br0
# inet 192.168.1.50/24 scope global br0

ip addr show enp0s3
# (sin IP — es un puerto del bridge)

bridge link show
# enp0s3: <BROADCAST,MULTICAST,UP,LOWER_UP> master br0

ping 192.168.1.1
# Debería funcionar — la red sigue operativa
```

### Diagrama de lo que pasó

```
ANTES:
  enp0s3 (192.168.1.50/24) ──── router (192.168.1.1)

DESPUÉS:
  br0 (192.168.1.50/24)
   │
   ├── enp0s3 (sin IP, puerto del bridge) ──── router (192.168.1.1)
   │
   └── (aquí conectarías vnetN de las VMs)
```

### Deshacer el bridge temporal

```bash
# Quitar la interfaz del bridge
sudo ip link set enp0s3 nomaster

# Eliminar el bridge
sudo ip link del br0

# Reasignar la IP a la interfaz física
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
# Qué interfaces están conectadas al bridge
bridge link show
# 2: enp0s3: <BROADCAST,MULTICAST,UP,LOWER_UP> master br0 state forwarding
# 8: vnet0: <BROADCAST,MULTICAST,UP,LOWER_UP> master br0 state forwarding
# 9: vnet1: <BROADCAST,MULTICAST,UP,LOWER_UP> master br0 state forwarding

# O filtrar por bridge
bridge link show br0

# Alternativa
ip link show master br0
```

### Estados de los puertos

| Estado | Significado |
|--------|------------|
| `disabled` | Puerto administrativamente deshabilitado |
| `listening` | STP: escuchando BPDUs (no reenvía aún) |
| `learning` | STP: aprendiendo MACs (no reenvía aún) |
| `forwarding` | Activo: reenviando tramas normalmente |
| `blocking` | STP: bloqueado para evitar loops |

En un lab simple sin STP activo o sin loops, todos los puertos estarán en `forwarding`.

### Estadísticas del bridge

```bash
# Tráfico por interfaz del bridge
ip -s link show br0
# RX: bytes  packets  errors  dropped
# TX: bytes  packets  errors  dropped

# Estadísticas detalladas
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
# 33:33:00:00:00:01 dev br0 self permanent    ← multicast, permanente

# Con brctl (formato diferente, misma info)
brctl showmacs br0
# port no   mac addr            is local?   ageing timer
#   1       08:00:27:bb:33:33   no           12.34
#   2       52:54:00:aa:11:11   no           45.67
#   3       52:54:00:aa:22:22   no            8.90
```

### Tipos de entradas FDB

| Tipo | Cómo se crea | Cuándo expira |
|------|-------------|---------------|
| `dynamic` (learned) | Automáticamente al ver tráfico | Después del aging time (300s) |
| `permanent` | Manualmente o por el kernel | Nunca (hasta que se borra) |
| `self` | Pertenece al bridge mismo | Permanente |

### Añadir una entrada estática (raro, pero posible)

```bash
# Forzar que una MAC siempre vaya por un puerto específico
sudo bridge fdb add 52:54:00:ff:ff:ff dev vnet0 master br0 permanent
```

### Limpiar la tabla FDB

```bash
# Borrar todas las entradas dinámicas (aprendidas)
sudo bridge fdb flush dev br0
```

---

## 6. Crear un bridge permanente con NetworkManager

NetworkManager es el gestor de red en Fedora, RHEL, Ubuntu Desktop, y la mayoría de distribuciones modernas. Un bridge creado con `nmcli` **sobrevive reinicios**.

### Paso a paso con nmcli

```bash
# Paso 1: Crear la conexión del bridge
sudo nmcli connection add type bridge \
    ifname br0 \
    con-name br0 \
    bridge.stp no

# Paso 2: Configurar IP (DHCP)
sudo nmcli connection modify br0 ipv4.method auto

# O IP estática:
sudo nmcli connection modify br0 \
    ipv4.method manual \
    ipv4.addresses 192.168.1.50/24 \
    ipv4.gateway 192.168.1.1 \
    ipv4.dns "8.8.8.8 1.1.1.1"

# Paso 3: Añadir la interfaz física como puerto del bridge
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

| Opción | Significado |
|--------|------------|
| `type bridge` | Crear una conexión de tipo bridge |
| `ifname br0` | Nombre de la interfaz del bridge |
| `con-name br0` | Nombre de la conexión en NetworkManager |
| `bridge.stp no` | Deshabilitar STP (para labs simples) |
| `type bridge-slave` | Crear un puerto del bridge |
| `master br0` | Este puerto pertenece al bridge br0 |

### Verificar la configuración persistente

```bash
# Los archivos de configuración se guardan en:
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

# Reactivar la conexión original de enp0s3 (si existía):
sudo nmcli connection up "Wired connection 1"
```

---

## 7. Crear un bridge permanente con systemd-networkd

En servidores minimalistas (Fedora CoreOS, Debian server sin NetworkManager, etc.), systemd-networkd gestiona la red.

### Archivos de configuración

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
# O estática:
# Address=192.168.1.50/24
# Gateway=192.168.1.1
# DNS=8.8.8.8
EOF
```

```bash
# 3. Añadir la interfaz física al bridge
sudo tee /etc/systemd/network/30-enp0s3.network << 'EOF'
[Match]
Name=enp0s3

[Network]
Bridge=br0
EOF
```

```bash
# Aplicar la configuración
sudo systemctl restart systemd-networkd

# Verificar
networkctl status br0
bridge link show
```

---

## 8. Crear un bridge permanente con netplan (Ubuntu)

Ubuntu Server usa netplan como capa de abstracción sobre NetworkManager o systemd-networkd:

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
      # O estática:
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

Una vez que tienes un bridge `br0` funcional, puedes conectar VMs de libvirt a él.

### Método 1: definir una red libvirt tipo bridge

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

### Método 2: conectar directamente al bridge (sin red libvirt)

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

### Qué cambia para la VM

| Aspecto | Con NAT (default) | Con bridge (br0) |
|---------|-------------------|-----------------|
| IP de la VM | 192.168.122.x (DHCP de dnsmasq) | 192.168.1.x (DHCP del router real) |
| Gateway | 192.168.122.1 (virbr0) | 192.168.1.1 (router real) |
| DNS | 192.168.122.1 (dnsmasq) | DNS del router real |
| Acceso desde la LAN | No (port forwarding) | Sí (IP en la misma red) |
| DHCP server | dnsmasq (libvirt) | Router doméstico |

### Permitir que qemu-bridge-helper acceda al bridge

Si usas QEMU sin libvirt, necesitas autorizar el bridge:

```bash
# /etc/qemu/bridge.conf (crear si no existe)
echo "allow br0" | sudo tee -a /etc/qemu/bridge.conf

# Verificar que qemu-bridge-helper tiene setuid
ls -la /usr/lib/qemu/qemu-bridge-helper
# -rwsr-xr-x 1 root root  ← el bit 's' es necesario
```

---

## 10. Configuración avanzada del bridge

### Parámetros del bridge

```bash
# Ver todos los parámetros
ip -d link show br0

# Parámetros accesibles via sysfs
ls /sys/class/net/br0/bridge/
# ageing_time        forward_delay      max_age
# bridge_id          hello_time         priority
# stp_state          ...
```

| Parámetro | Default | Significado |
|-----------|---------|------------|
| `ageing_time` | 30000 (300s) | Tiempo antes de olvidar una MAC inactiva |
| `stp_state` | 1 (on) | Spanning Tree Protocol activo |
| `forward_delay` | 1500 (15s) | Tiempo de espera STP antes de reenviar |
| `hello_time` | 200 (2s) | Intervalo de mensajes hello de STP |
| `max_age` | 2000 (20s) | Tiempo máximo para considerar un BPDU válido |
| `priority` | 32768 | Prioridad del bridge para elección de root bridge |

Los valores en sysfs están en centésimas de segundo (jiffies).

### Modificar parámetros

```bash
# Desactivar STP (para labs simples)
sudo ip link set br0 type bridge stp_state 0

# Reducir forward_delay (arranque más rápido)
sudo ip link set br0 type bridge forward_delay 0

# Cambiar aging time
sudo ip link set br0 type bridge ageing_time 12000    # 120 segundos
```

### Filtrado de VLANs (avanzado)

Linux bridge soporta VLAN filtering para segmentar tráfico:

```bash
# Habilitar VLAN filtering
sudo ip link set br0 type bridge vlan_filtering 1

# Asignar VLAN 100 a un puerto
sudo bridge vlan add vid 100 dev vnet0

# Ver VLANs por puerto
bridge vlan show
```

Para este curso no usaremos VLANs en bridges, pero es útil saber que la capacidad existe.

---

## 11. El bridge virbr0 de libvirt vs un bridge manual

Es común confundir virbr0 (el bridge de la red NAT de libvirt) con un bridge manual conectado a la red física. Son conceptos diferentes:

### virbr0 (bridge NAT de libvirt)

```
VM (.122.100) → vnet0 → virbr0 → [NAT/MASQUERADE] → enp0s3 → router
                         │
                         │ IP: 192.168.122.1
                         │ Función: bridge + gateway + DHCP + DNS + NAT
```

- Creado y gestionado por libvirt automáticamente.
- **Tiene NAT encima**: virbr0 es un bridge, pero el tráfico que sale de él pasa por iptables MASQUERADE.
- **Tiene su propia red**: 192.168.122.0/24 (diferente a la LAN).
- **Incluye servicios**: DHCP (dnsmasq), DNS (dnsmasq), NAT (iptables).
- **No** está conectado a enp0s3 como puerto del bridge.

### br0 (bridge manual conectado a la interfaz física)

```
VM (.1.150) → vnet0 → br0 → enp0s3 → router
                       │
                       │ IP: 192.168.1.50 (la IP del host, movida aquí)
                       │ Función: solo bridge (switch virtual)
```

- Creado por ti manualmente (o con NetworkManager).
- **No tiene NAT**: el tráfico pasa directo a la red física.
- **Misma red que la LAN**: las VMs obtienen IPs del DHCP del router real.
- **No incluye servicios**: no tiene DHCP ni DNS propios.
- **enp0s3 es un puerto** del bridge (la interfaz física pierde su IP).

### Tabla comparativa

| Aspecto | virbr0 (libvirt NAT) | br0 (bridge manual) |
|---------|---------------------|-------------------|
| Quién lo crea | libvirt automáticamente | Tú (nmcli, ip link, etc.) |
| Red | 192.168.122.0/24 (privada) | La red del host (192.168.1.0/24) |
| NAT | Sí (MASQUERADE) | No |
| DHCP | dnsmasq (192.168.122.2-254) | Router real de la LAN |
| DNS | dnsmasq (forwarding) | DNS del router/ISP |
| Interfaz física | No conectada | enp0s3 es puerto del bridge |
| VM visible en LAN | No | Sí |
| Gestión | `virsh net-*` | `nmcli`, `ip link`, archivos de config |

---

## 12. Errores comunes

### Error 1: Crear un bridge por SSH y perder la conexión

```bash
# Estás conectado por SSH a 192.168.1.50 (enp0s3)
sudo ip link set enp0s3 master br0
# → enp0s3 pierde su IP
# → SSH se congela
# → No puedes arreglar nada remotamente
```

**Prevención:**

```bash
# Opción A: hacer todo en un solo comando
sudo ip addr flush dev enp0s3 && \
sudo ip link set enp0s3 master br0 && \
sudo ip addr add 192.168.1.50/24 dev br0 && \
sudo ip route add default via 192.168.1.1 dev br0

# Opción B: usar nmcli (gestiona la transición por ti)
sudo nmcli connection add type bridge ifname br0 con-name br0
sudo nmcli connection add type bridge-slave ifname enp0s3 master br0
sudo nmcli connection up br0

# Opción C: hacerlo desde la consola local (no SSH)
```

### Error 2: El bridge existe pero no tiene IP

```bash
ip addr show br0
# NO hay línea "inet ..."

ping 192.168.1.1
# connect: Network is unreachable
```

El bridge necesita una IP para que el host tenga conectividad:

```bash
# DHCP:
sudo dhclient br0

# Estática:
sudo ip addr add 192.168.1.50/24 dev br0
sudo ip route add default via 192.168.1.1 dev br0
```

### Error 3: NetworkManager reconfigura todo al activarse

```bash
# Creas un bridge con ip link (temporal)
# NetworkManager ve la interfaz "suelta" y la reclama
# Tu configuración desaparece
```

Soluciones:

```bash
# Opción A: decirle a NM que no toque la interfaz
sudo nmcli device set enp0s3 managed no

# Opción B: usar nmcli para crear el bridge (NM lo gestiona correctamente)
# (ver sección 6)

# Opción C: desactivar NM completamente (solo para servidores)
sudo systemctl disable --now NetworkManager
```

### Error 4: La VM obtiene IP del rango equivocado

```bash
# Esperabas que la VM obtuviera IP de la LAN (192.168.1.x)
# Pero obtiene 192.168.122.x

# Causa: la VM sigue conectada a la red NAT, no al bridge
virsh domiflist mi-vm
# Si dice "source: default" → está en NAT, no en el bridge

# Solución:
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
#                             ← falta UP

sudo ip link set br0 up
# Ahora: <BROADCAST,MULTICAST,UP,LOWER_UP> state UP
```

---

## 13. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                    BRIDGE EN LINUX CHEATSHEET                       ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CREAR BRIDGE (TEMPORAL — iproute2)                                ║
║  ip link add br0 type bridge          Crear bridge                 ║
║  ip link set br0 up                   Activar bridge               ║
║  ip link set enp0s3 master br0        Añadir interfaz al bridge    ║
║  ip addr add 192.168.1.50/24 dev br0  Asignar IP al bridge        ║
║  ip route add default via 192.168.1.1 Agregar ruta por defecto    ║
║  ip link set enp0s3 nomaster          Quitar interfaz del bridge   ║
║  ip link del br0                      Eliminar bridge              ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CREAR BRIDGE (PERMANENTE — NetworkManager)                        ║
║  nmcli con add type bridge ifname br0 con-name br0                 ║
║  nmcli con add type bridge-slave ifname enp0s3 master br0          ║
║  nmcli con modify br0 ipv4.method auto    (DHCP)                   ║
║  nmcli con up br0                                                  ║
║  nmcli con delete br0                     (eliminar)               ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  INSPECCIÓN                                                        ║
║  ip link show type bridge        Listar bridges                    ║
║  bridge link show                Ver puertos de cada bridge        ║
║  bridge fdb show br br0          Ver tabla MAC (FDB)               ║
║  ip -d link show br0             Parámetros del bridge             ║
║  ip -s link show br0             Estadísticas de tráfico           ║
║  bridge link show br0            Estado de puertos                  ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CONECTAR VMs DE LIBVIRT AL BRIDGE                                 ║
║  virsh edit <vm>                                                   ║
║    <interface type='bridge'>                                       ║
║      <source bridge='br0'/>                                        ║
║      <model type='virtio'/>                                        ║
║    </interface>                                                     ║
║                                                                    ║
║  O definir red libvirt tipo bridge:                                ║
║    <forward mode='bridge'/> <bridge name='br0'/>                   ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  virbr0 vs br0                                                     ║
║  virbr0:  bridge + NAT + DHCP + DNS (libvirt, red privada)         ║
║  br0:     bridge puro (manual, misma red que la LAN)               ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  TROUBLESHOOTING                                                   ║
║  Bridge sin IP → ip addr add / dhclient br0                       ║
║  SSH se cuelga → hacer todo en un solo comando / usar nmcli        ║
║  NM interfiere → nmcli device set <if> managed no                  ║
║  VM en red equivocada → virsh domiflist / virsh edit               ║
║  Bridge DOWN → ip link set br0 up                                  ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 14. Ejercicios

### Ejercicio 1: Bridge de prueba (aislado, sin riesgo)

Crea un bridge de prueba sin tocar tu red real:

```bash
# 1. Crear bridge aislado
sudo ip link add br-lab type bridge
sudo ip link set br-lab up
sudo ip addr add 10.99.0.1/24 dev br-lab

# 2. Verificar
ip link show type bridge
ip addr show br-lab
bridge link show br-lab
```

**Preguntas:**
1. ¿El bridge tiene interfaces miembro? ¿Por qué no?
2. ¿Puedes hacer `ping 10.99.0.1`? ¿Desde dónde estás haciendo ping? (pista: el bridge está en tu host, así que 10.99.0.1 es una IP del host).
3. Si conectaras una VM a este bridge, ¿tendría internet? ¿Por qué?
4. ¿Qué necesitarías añadir para que las VMs en br-lab tengan internet? (pista: hay dos opciones: NAT o conectar enp0s3 al bridge).

Limpia al terminar:
```bash
sudo ip link del br-lab
```

### Ejercicio 2: Inspección de virbr0

Si tienes libvirt instalado, examina virbr0 en detalle:

```bash
ip -d link show virbr0
ip addr show virbr0
bridge link show virbr0
bridge fdb show br virbr0
cat /sys/class/net/virbr0/bridge/stp_state
cat /sys/class/net/virbr0/bridge/ageing_time
```

**Preguntas:**
1. ¿STP está activado en virbr0? ¿El valor es 0 o 1?
2. ¿Cuál es el aging time? Conviértelo de centésimas de segundo a segundos.
3. ¿La tabla FDB (bridge fdb) tiene entradas? Si hay VMs corriendo, ¿ves sus MACs? Si no hay VMs, ¿qué entradas ves?
4. ¿virbr0 tiene un puerto conectado a enp0s3? ¿Por qué no? (pista: es un bridge de NAT, no un bridge a la red física).

### Ejercicio 3: Planificación de bridge para un lab

Quieres crear un lab donde tres VMs (web, api, db) sean accesibles desde tu LAN para que tus compañeros puedan probar el sistema. Tu host tiene:
- enp0s3: 192.168.1.50/24 (LAN doméstica, gateway 192.168.1.1)
- virbr0: 192.168.122.1/24 (NAT de libvirt, ya existente)

Responde sin ejecutar:

1. ¿Crearías un bridge nuevo (br0) o reutilizarías virbr0? ¿Por qué?
2. Escribe los comandos `nmcli` para crear br0 con enp0s3 como puerto e IP estática 192.168.1.50/24.
3. Después de crear br0, ¿enp0s3 tendría IP? ¿Dónde estaría la IP del host?
4. Escribe el fragmento XML de `<interface>` para conectar una VM al bridge br0.
5. ¿Las VMs obtendrían IP de dnsmasq (libvirt) o del DHCP de tu router? ¿Cómo sabrías qué IPs les asignó?
6. ¿virbr0 seguiría funcionando? ¿Podrías tener VMs en NAT y VMs en bridge al mismo tiempo?

**Pregunta reflexiva:** Si tu host solo tiene WiFi (wlp2s0) y no tiene interfaz Ethernet, ¿podrías crear br0 con wlp2s0 como puerto? ¿Qué alternativa usarías para hacer las VMs accesibles desde la LAN?

---

> **Nota**: crear un bridge en Linux es técnicamente simple — unos pocos comandos. La complejidad real está en no perder la conectividad del host durante el proceso (especialmente por SSH), en elegir entre configuración temporal y permanente, y en entender la diferencia entre virbr0 (bridge+NAT de libvirt) y un bridge manual (bridge puro conectado a la interfaz física). Para labs donde solo necesitas que las VMs tengan internet, NAT es más simple y seguro. Para labs donde otros dispositivos necesitan alcanzar las VMs, el bridge es necesario.
