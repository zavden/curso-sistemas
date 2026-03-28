# T02 — Gestión de Redes Virtuales con virsh

## Comandos Esenciales de virsh net-*

Los comandos `virsh net-*` gestionan redes virtuales (no VMs, sino la red entre ellas).

## Listar y Ver Redes

```bash
# Listar todas las redes (activas e inactivas)
virsh net-list --all

# Nombre               Estado   Inicio   Persistente
# ----------------------------------------------------------
# default              activo   sí       sí
# isolated             inactivo  no       sí
```

```bash
# Información detallada de una red
virsh net-info default

# Nombre:     default
# UUID:       xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
# Activo:     sí
# Persistente: sí
# Autoinicio: sí
# Bridge:     virbr0
```

```bash
# Ver el XML completo de la red
virsh net-dumpxml default

# Ver los leases DHCP activos
virsh net-dhcp-leases default
```

## Iniciar, Detener, Reiniciar

```bash
# Iniciar una red
virsh net-start default

# Detener una red (las VMs pierden conectividad)
virsh net-destroy default

# Reiniciar
virsh net-destroy default && virsh net-start default
```

⚠️ `net-destroy` es como apagar el switch — las VMs pierden conexión inmediatamente.

## Autostart (inicio automático)

```bash
# Habilitar inicio automático
virsh net-autostart default

# Deshabilitar
virsh net-autostart --disable default

# Verificar
virsh net-info default | grep Autoinicio
```

Cuando el host arranca, la red con autostart se levanta automáticamente.

## Editar una Red

```bash
# Abrir editor con el XML actual
virsh net-edit default

# Ejemplo: cambiar el rango DHCP
# Buscar:
#   <range start='192.168.122.2' end='192.168.122.254'/>
# Cambiar a:
#   <range start='192.168.122.10' end='192.168.122.200'/>
```

Tras editar, reiniciar la red:

```bash
virsh net-destroy default && virsh net-start default
```

## Crear una Nueva Red (Ejemplo: Aislada)

### Opción 1: Desde XML

```bash
cat > /tmp/isolated-net.xml <<'EOF'
<network>
  <name>isolated</name>
  <uuid>xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx</uuid>
  <forward mode='none'/>
  <bridge name='virbr1' stp='on' delay='0'/>
  <ip address='10.0.1.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.0.1.2' end='10.0.1.254'/>
    </dhcp>
  </ip>
</network>
EOF

virsh net-define /tmp/isolated-net.xml
virsh net-start isolated
virsh net-autostart isolated
```

### Opción 2: Con virsh net-define-as

```bash
virsh net-define-as isolated --opzione "none" \
  --bridge virbr1 --ip 10.0.1.1 --netmask 255.255.255.0 \
  --dhcp-start 10.0.1.2 --dhcp-end 10.0.1.254
```

## Eliminar una Red

```bash
# Primero detenerla
virsh net-destroy isolated

# Luego desregistrar (la redefine si es persistente)
virsh net-undefine isolated
```

## Asignar una Red a una VM

```bash
# Ver qué red usa una VM
virsh dumpxml debian-lab | grep '<interface'

# resultado:
# <interface type='network'>
#   <source network='default'/>
```

### Cambiar la red de una VM

```bash
# Apagar la VM
virsh shutdown debian-lab

# Editar la interfaz
virsh edit debian-lab
```

Cambiar:

```xml
<interface type='network'>
  <source network='default'/>
```

a:

```xml
<interface type='network'>
  <source network='isolated'/>
```

O usar attach-interface para añadir una segunda interfaz:

```bash
# Añadir interfaz de red aislada a una VM
virsh attach-interface debian-lab network isolated --persistent
```

## Ver Estado de la Red desde la VM

Dentro de la VM:

```bash
# Ver IP
ip addr show

# Ver gateway
ip route show

# Probar conectividad
ping 8.8.8.8           # internet
ping 192.168.122.1      # gateway
ping 192.168.122.101    # otra VM
```

## Diagnóstico

```bash
# ¿Qué redes hay?
virsh net-list --all

# ¿Está activa la red?
ip link show virbr0

# ¿Tiene IP?
ip addr show virbr0

# ¿Hay leases DHCP?
virsh net-dhcp-leases default

# ¿Qué VMs están conectadas?
virsh list --all | while read vm rest; do
  virsh dumpxml "$vm" 2>/dev/null | grep -l "network='default'"
done

# Logs de libvirt
tail -f /var/log/libvirt/libvirtd.log
```

## Caso de Uso en el Curso

### Setup para B08 (Almacenamiento)

```bash
# Crear red aislada para labs de almacenamiento
cat > /tmp/lab-isolated.xml <<'EOF'
<network>
  <name>lab-isolated</name>
  <forward mode='none'/>
  <bridge name='virbr10' stp='on' delay='0'/>
  <ip address='10.0.10.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.0.10.2' end='10.0.10.50'/>
    </dhcp>
  </ip>
</network>
EOF

virsh net-define /tmp/lab-isolated.xml
virsh net-start lab-isolated
virsh net-autostart lab-isolated

# Ahora VMs con --network network=lab-isolated
# están aisladas — pueden usar discos extra sin riesgo
```

### Cleanup después del lab

```bash
virsh net-destroy lab-isolated
virsh net-undefine lab-isolated
```

## Ejercicios

1. Ejecuta `virsh net-list --all` en tu sistema. ¿Cuántas redes hay?
   ¿Cuál es la red por defecto?

2. Ver los leases DHCP de la red default:
   ```bash
   virsh net-dhcp-leases default
   ```
   Si no hay VMs corriendo, no habrá leases.

3. Crear una red aislada temporal (para practicar):
   ```bash
   # Definir
   cat > /tmp/test-iso.xml <<'EOF'
   <network>
     <name>test-iso</name>
     <forward mode='none'/>
     <bridge name='virbr99' stp='on' delay='0'/>
     <ip address='10.99.99.1' netmask='255.255.255.0'>
       <dhcp>
         <range start='10.99.99.2' end='10.99.99.50'/>
       </dhcp>
     </ip>
   </network>
   EOF
   virsh net-define /tmp/test-iso.xml
   
   # Iniciar
   virsh net-start test-iso
   
   # Limpiar al final:
   virsh net-destroy test-iso && virsh net-undefine test-iso
   ```

4. Investiga: ¿qué pasa si intentas `virsh net-destroy default`
   mientras tienes VMs corriendo en esa red?

## Resumen

| Comando | Función |
|---|---|
| `virsh net-list --all` | Listar todas las redes |
| `virsh net-info <red>` | Información de una red |
| `virsh net-dumpxml <red>` | Ver XML completo |
| `virsh net-dhcp-leases <red>` | Ver leases DHCP activos |
| `virsh net-start <red>` | Iniciar red |
| `virsh net-destroy <red>` | Detener red |
| `virsh net-autostart <red>` | Habilitar inicio automático |
| `virsh net-edit <red>` | Editar XML de la red |
| `virsh net-define <archivo.xml>` | Crear red desde XML |
| `virsh net-undefine <red>` | Eliminar definición de red |
| `virsh attach-interface <vm> network <red>` | Añadir interfaz de red a VM |
