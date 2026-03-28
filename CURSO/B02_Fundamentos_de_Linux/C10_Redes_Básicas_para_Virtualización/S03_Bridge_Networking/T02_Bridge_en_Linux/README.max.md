# T02 — Bridge en Linux

## El Módulo bridge del Kernel

Linux tiene soporte nativo para bridge como **módulo del kernel** (o compilado directamente):

```bash
lsmod | grep bridge
bridge  286720  0
stp     16384  1 bridge
```

- `bridge`: módulo principal
- `stp`: Spanning Tree Protocol — evita loops en redes con múltiples bridges

## Paquetes Necesarios

```bash
# Debian/Ubuntu
apt install bridge-utils

# Fedora/RHEL
dnf install bridge-utils
```

`bridge-utils` provee `brctl`, el comando clásico para gestionar bridges.

## Crear un Bridge Manual (brctl)

### Paso 1: Crear el bridge

```bash
brctl addbr br0
ip link set br0 up
```

### Paso 2: Añadir interfaces al bridge

```bash
# Supongamos que eth0 es la interfaz física
brctl addif br0 eth0
```

**Advertencia**: al añadir una interfaz física al bridge, **esa interfaz pierde su IP**. El bridge toma el control de la red.

```bash
# ANTES: eth0 tiene IP
ip addr show eth0
# inet 192.168.1.100/24

# Crear bridge y añadir eth0
brctl addbr br0
ip link set eth0 master br0
ip link set eth0 up
ip link set br0 up

# DESPUÉS: eth0 no tiene IP, br0 sí
ip addr show eth0
# (sin IP)
ip addr show br0
# inet 192.168.1.100/24
```

### Paso 3: Asignar IP al bridge

```bash
ip addr add 192.168.1.100/24 dev br0
ip route add default via 192.168.1.1
```

## bridge-utils: `brctl`

```bash
# Ver todos los bridges
brctl show

# Añadir interfaz
brctl addif br0 eth0

# Quitar interfaz
brctl delif br0 eth0

# Eliminar bridge (primero eliminar interfaces)
ip link set br0 down
brctl delbr br0

# Ver tabla MAC aprendida
brctl showmacs br0

# Ver spanning tree
brctl showstp br0
```

## iproute2: `ip link` (moderno, preferido)

Desde Linux moderno, `iproute2` puede hacer todo sin `brctl`:

```bash
# Crear bridge
ip link add br0 type bridge

# Ponerlo up
ip link set br0 up

# Añadir interfaz
ip link set eth0 master br0

# Ver bridges
bridge link

# Ver detalles del bridge
bridge link show

# Quitar interfaz del bridge
ip link set eth0 nomaster
```

## Bridge en libvirt

En libvirt, crear un bridge para VMs es más sencillo:

### Bridge externo (conectado a la red física)

Definido en `/etc/qemu/bridge.conf`:

```
allow br0
```

O crear la red en libvirt:

```bash
# Crear un XML de red tipo bridge
cat > /tmp/bridge-net.xml <<'EOF'
<network>
  <name>host-bridge</name>
  <forward mode='bridge'/>
  <bridge name='br0'/>
</network>
EOF

virsh net-define /tmp/bridge-net.xml
virsh net-start host-bridge
virsh net-autostart host-bridge
```

### Usar el bridge en una VM

```bash
# Al crear la VM
virt-install \
  --name debian-lab \
  --network type=bridge,source=br0 \
  ...

# O modificar una VM existente
virsh edit debian-lab
```

Cambiar:

```xml
<interface type='network'>
  <source network='default'/>
```

a:

```xml
<interface type='bridge'>
  <source bridge='br0'/>
```

## Configurar bridge permanente (NetworkManager)

### nmcli

```bash
# Crear bridge
nmcli con add type bridge ifname br0 con-name br0

# Añadir la interfaz física
nmcli con add type bridge-slave ifname eth0 master br0

# Configurar IP (DHCP o estática)
nmcli con mod br0 ipv4.method auto      # DHCP
# O estática:
nmcli con mod br0 ipv4.addresses 192.168.1.100/24
nmcli con mod br0 ipv4.gateway 192.168.1.1
nmcli con mod br0 ipv4.dns 8.8.8.8
nmcli con mod br0 ipv4.method manual

# Activar
nmcli con up br0
```

## Problemas Comunes con Bridge

### Problema 1: Sin conexión después de crear bridge

```bash
# La interfaz física perdió la IP
ip addr show eth0   # ¿Tiene IP?
ip addr show br0    # ¿La tiene br0?
```

Solución: asignar IP al bridge, no a la interfaz física.

### Problema 2: NetworkManager interfiere

```bash
# Desconectar la interfaz física de NetworkManager
nmcli device set eth0 managed no
```

### Problema 3: El bridge no reenvía tráfico

```bash
# Verificar que el bridge está up y tiene puertos
ip link show master br0

# Ver estadísticas
ip -s link show br0
bridge link
```

## Verificación

```bash
# ¿El bridge existe?
ip link show type bridge

# ¿ eth0 está en el bridge?
ip link show master eth0

# Tabla MAC
bridge fdb show

# ¿El bridge tiene IP?
ip addr show br0
```

## Ejercicios

1. **En el host (si tienes eth0 adicional o estás en una VM de test)**:
   ```bash
   # Crear un bridge temporal (no hagas esto en producción sin eth0 extra)
   ip link add br-test type bridge
   ip link set br-test up
   brctl show
   ip link show type bridge
   # Limpiar:
   ip link del br-test
   ```

2. Verifica el bridge de libvirt:
   ```bash
   ip link show virbr0
   bridge link show dev virbr0
   # ¿Qué interfaz está conectada a virbr0?
   ```

3. En el syllabus de C00 (QEMU), se menciona bridge en S06 T03.
   Investiga: ¿cuál es la diferencia entre el bridge de libvirt (`virbr0`)
   y un bridge manual (`br0`)?

4. **Investiga** (no ejecutar): si quisieras hacer bridge en tu laptop WiFi,
   ¿por qué es complicado? (Pista: buscar "bridge wifi interface linux")

## Resumen

| Comando | Función |
|---|---|
| `brctl addbr br0` | Crear bridge |
| `brctl addif br0 eth0` | Añadir interfaz al bridge |
| `brctl delif br0 eth0` | Quitar interfaz del bridge |
| `brctl delbr br0` | Eliminar bridge |
| `ip link add br0 type bridge` | Crear bridge (iproute2 moderno) |
| `ip link set eth0 master br0` | Añadir interfaz (iproute2) |
| `bridge link` | Ver interfaces en bridges |
| libvirt | Usa bridge para VMs visibles en la red física |
