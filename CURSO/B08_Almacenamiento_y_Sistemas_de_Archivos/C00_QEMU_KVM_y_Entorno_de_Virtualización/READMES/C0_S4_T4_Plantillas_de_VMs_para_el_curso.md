# Plantillas de VMs para el curso

## Índice

1. [Por qué necesitamos plantillas](#por-qué-necesitamos-plantillas)
2. [Estrategia: dos plantillas base](#estrategia-dos-plantillas-base)
3. [Crear la plantilla Debian 12](#crear-la-plantilla-debian-12)
4. [Crear la plantilla AlmaLinux 9](#crear-la-plantilla-almalinux-9)
5. [Deshabilitar cloud-init](#deshabilitar-cloud-init)
6. [Sellar y convertir a plantilla](#sellar-y-convertir-a-plantilla)
7. [Clonar desde plantilla: virt-clone](#clonar-desde-plantilla-virt-clone)
8. [Clonar desde plantilla: overlays](#clonar-desde-plantilla-overlays)
9. [Script de provisioning](#script-de-provisioning)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## Por qué necesitamos plantillas

A lo largo del curso crearemos decenas de VMs para diferentes labs:
filesystems, LVM, RAID, networking, etc. Cada vez necesitamos una VM con
el mismo punto de partida:

```
Sin plantilla:                      Con plantilla:

  Lab ext4:  cloud-init + config     Lab ext4:  overlay de plantilla
  Lab xfs:   cloud-init + config     Lab xfs:   overlay de plantilla
  Lab LVM:   cloud-init + config     Lab LVM:   overlay de plantilla
  Lab RAID:  cloud-init + config     Lab RAID:  overlay de plantilla
  ...                                ...

  Cada VM: ~60s + seed individual    Cada VM: ~0.1s + arrancar
  Repetir cloud-init cada vez        cloud-init ya no interviene
  Mantener archivos seed             Solo overlays desechables
```

Una **plantilla** es una imagen qcow2 con el SO completamente configurado,
cloud-init deshabilitado, y marcada como read-only. A partir de ella se
generan overlays instantáneos para cada lab.

---

## Estrategia: dos plantillas base

Para cubrir ambas familias de distribuciones del curso:

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  Plantilla Debian 12              Plantilla AlmaLinux 9     │
│  (tpl-debian12.qcow2)            (tpl-alma9.qcow2)         │
│                                                             │
│  • labuser con sudo               • labuser con sudo        │
│  • root accesible                  • root accesible          │
│  • SSH habilitado                  • SSH habilitado          │
│  • vim, curl, tmux,               • vim, curl, tmux,        │
│    bash-completion, man-db          bash-completion, man     │
│  • cloud-init deshabilitado        • cloud-init deshabilitado│
│  • Sistema actualizado             • Sistema actualizado     │
│  • Tamaño disco: 20 GiB           • Tamaño disco: 20 GiB   │
│  • Permisos: 444 (read-only)      • Permisos: 444           │
│                                                             │
│        │                                   │                │
│        ├── lab_ext4.qcow2 (overlay)        ├── lab_lvm.qcow2│
│        ├── lab_xfs.qcow2  (overlay)        ├── lab_raid.qcow│
│        ├── lab_btrfs.qcow2 (overlay)       └── ...          │
│        └── ...                                              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

Configuración común en ambas plantillas:

| Aspecto | Valor |
|---------|-------|
| Usuario | `labuser` / `labpass123` |
| Root | `labpass123` |
| Sudo | sin contraseña para labuser |
| SSH | habilitado, password auth activado |
| Paquetes | vim, curl, tmux, bash-completion, man-db/man-pages, htop |
| cloud-init | deshabilitado |
| Consola serial | configurada en GRUB |

---

## Crear la plantilla Debian 12

Usamos cloud-init (T03) para la creación inicial, y luego sellamos:

### Paso 1: Preparar imagen y seed

```bash
cd /var/lib/libvirt/images/

# Overlay desde la imagen cloud
sudo qemu-img create -f qcow2 \
    -b debian-12-generic-amd64.qcow2 -F qcow2 \
    tpl-debian12.qcow2 20G

# meta-data
cat > /tmp/meta-data-tpl-deb <<'EOF'
instance-id: tpl-debian12
local-hostname: debian-tpl
EOF

# user-data
cat > /tmp/user-data-tpl-deb <<'EOF'
#cloud-config
hostname: debian-tpl

users:
  - name: labuser
    plain_text_passwd: labpass123
    lock_passwd: false
    sudo: ALL=(ALL) NOPASSWD:ALL
    shell: /bin/bash
    groups: [adm, sudo]

chpasswd:
  list: |
    root:labpass123
  expire: false

ssh_pwauth: true
disable_root: false

package_update: true
package_upgrade: true
packages:
  - vim
  - curl
  - tmux
  - bash-completion
  - man-db
  - htop
  - less
  - net-tools
  - lsof
  - strace

timezone: Europe/Madrid

# Consola serial para GRUB
bootcmd:
  - sed -i 's/GRUB_CMDLINE_LINUX_DEFAULT="quiet"/GRUB_CMDLINE_LINUX_DEFAULT="quiet console=ttyS0,115200n8"/' /etc/default/grub
  - update-grub

runcmd:
  - echo "Plantilla Debian 12 preparada: $(date)" > /root/tpl-info.txt
  - systemctl enable ssh

final_message: "Plantilla lista después de $UPTIME segundos"
EOF

# Generar ISO seed
cloud-localds /tmp/seed-tpl-deb.iso \
    /tmp/user-data-tpl-deb /tmp/meta-data-tpl-deb
sudo mv /tmp/seed-tpl-deb.iso /var/lib/libvirt/images/
```

### Paso 2: Arrancar y dejar que cloud-init configure

```bash
sudo virt-install \
    --name tpl-debian12-setup \
    --ram 2048 \
    --vcpus 2 \
    --import \
    --disk path=/var/lib/libvirt/images/tpl-debian12.qcow2 \
    --disk path=/var/lib/libvirt/images/seed-tpl-deb.iso,device=cdrom \
    --os-variant debian12 \
    --network network=default \
    --graphics none \
    --noautoconsole
```

### Paso 3: Verificar y ajustar

```bash
# Esperar a que cloud-init termine (~1-2 minutos por el upgrade)
sleep 90
virsh console tpl-debian12-setup
# Login: labuser / labpass123
```

Dentro de la VM, verificar:

```bash
# ¿cloud-init terminó?
cloud-init status
# status: done

# ¿Paquetes instalados?
dpkg -l vim curl tmux htop | grep ^ii | wc -l
# 4

# ¿SSH funcionando?
systemctl is-enabled ssh
# enabled

# ¿Consola serial en GRUB?
grep console /etc/default/grub
# GRUB_CMDLINE_LINUX_DEFAULT="quiet console=ttyS0,115200n8"

# ¿sudo sin contraseña?
sudo whoami
# root  (sin pedir contraseña)
```

Si algo falta, instalarlo o configurarlo manualmente ahora. Esta es la
última oportunidad antes de sellar la plantilla.

```bash
# Salir de la consola
# Ctrl+]
```

---

## Crear la plantilla AlmaLinux 9

Mismo flujo, adaptado para AlmaLinux:

### Preparar

```bash
cd /var/lib/libvirt/images/

sudo qemu-img create -f qcow2 \
    -b AlmaLinux-9-GenericCloud-latest.x86_64.qcow2 -F qcow2 \
    tpl-alma9.qcow2 20G

cat > /tmp/meta-data-tpl-alma <<'EOF'
instance-id: tpl-alma9
local-hostname: alma-tpl
EOF

cat > /tmp/user-data-tpl-alma <<'EOF'
#cloud-config
hostname: alma-tpl

users:
  - name: labuser
    plain_text_passwd: labpass123
    lock_passwd: false
    sudo: ALL=(ALL) NOPASSWD:ALL
    shell: /bin/bash
    groups: [adm, wheel]

chpasswd:
  list: |
    root:labpass123
  expire: false

ssh_pwauth: true
disable_root: false

package_update: true
package_upgrade: true
packages:
  - vim-enhanced
  - curl
  - tmux
  - bash-completion
  - man-pages
  - htop
  - less
  - net-tools
  - lsof
  - strace

timezone: Europe/Madrid

bootcmd:
  - |
    grep -q console /etc/default/grub 2>/dev/null || \
    sed -i 's/GRUB_CMDLINE_LINUX="/GRUB_CMDLINE_LINUX="console=ttyS0,115200n8 /' /etc/default/grub
  - grub2-mkconfig -o /boot/grub2/grub.cfg

runcmd:
  - echo "Plantilla AlmaLinux 9 preparada: $(date)" > /root/tpl-info.txt
  - systemctl enable sshd

final_message: "Plantilla lista después de $UPTIME segundos"
EOF

cloud-localds /tmp/seed-tpl-alma.iso \
    /tmp/user-data-tpl-alma /tmp/meta-data-tpl-alma
sudo mv /tmp/seed-tpl-alma.iso /var/lib/libvirt/images/
```

### Arrancar

```bash
sudo virt-install \
    --name tpl-alma9-setup \
    --ram 2048 \
    --vcpus 2 \
    --import \
    --disk path=/var/lib/libvirt/images/tpl-alma9.qcow2 \
    --disk path=/var/lib/libvirt/images/seed-tpl-alma.iso,device=cdrom \
    --os-variant almalinux9 \
    --network network=default \
    --graphics none \
    --noautoconsole
```

### Verificar

```bash
sleep 120  # AlmaLinux tarda un poco más con dnf update
virsh console tpl-alma9-setup
# Login: labuser / labpass123

# Dentro de la VM:
cloud-init status
rpm -q vim-enhanced curl tmux htop
systemctl is-enabled sshd
sudo whoami

# Ctrl+]
```

---

## Deshabilitar cloud-init

Antes de sellar la plantilla, **deshabilitamos cloud-init**. Si no lo
hacemos, cada VM clonada desde la plantilla intentará ejecutar cloud-init
al arrancar, buscando un datasource que no existe y tardando ~120 segundos.

### Dentro de la VM (antes de apagar)

```bash
virsh console tpl-debian12-setup
# Login como root
```

**Método 1: archivo de bloqueo** (recomendado, limpio y reversible)

```bash
sudo touch /etc/cloud/cloud-init.disabled
```

cloud-init comprueba este archivo al arrancar. Si existe, se desactiva
completamente sin necesidad de desinstalar nada.

**Método 2: deshabilitar servicios systemd**

```bash
sudo systemctl disable cloud-init-local.service
sudo systemctl disable cloud-init.service
sudo systemctl disable cloud-config.service
sudo systemctl disable cloud-final.service
```

**Método 3: desinstalar** (más agresivo, no recomendado para plantillas)

```bash
# Debian
sudo apt purge cloud-init -y

# AlmaLinux
sudo dnf remove cloud-init -y
```

Preferimos el método 1 porque si algún día necesitas re-ejecutar cloud-init
en una VM derivada, basta con borrar el archivo.

### Limpiar datos de cloud-init

```bash
# Eliminar los datos de la instancia actual
sudo cloud-init clean

# Verificar que se limpió
ls /var/lib/cloud/
# El directorio debería estar mayormente vacío
```

`cloud-init clean` elimina los datos de la ejecución actual (logs,
instance-id almacenado, scripts ejecutados). Esto es importante porque si
no limpiamos y alguien borrara el archivo `cloud-init.disabled`, cloud-init
creería que ya se ejecutó y no haría nada (por el instance-id almacenado).

```bash
# Salir
# Ctrl+]
```

Repetir los mismos pasos en la VM de AlmaLinux.

---

## Sellar y convertir a plantilla

"Sellar" una plantilla significa prepararla para clonación: limpiar datos
únicos (machine-id, SSH host keys, logs) y marcar como read-only.

### Paso 1: Limpiar dentro de la VM

Conectar a la VM y ejecutar la limpieza:

```bash
virsh console tpl-debian12-setup

# Dentro de la VM, como root:

# Limpiar machine-id (se regenerará en cada clon)
sudo truncate -s 0 /etc/machine-id
sudo rm -f /var/lib/dbus/machine-id

# Limpiar SSH host keys (se regenerarán al primer boot)
sudo rm -f /etc/ssh/ssh_host_*

# Limpiar logs
sudo truncate -s 0 /var/log/wtmp
sudo truncate -s 0 /var/log/lastlog
sudo journalctl --rotate
sudo journalctl --vacuum-time=1s

# Limpiar historial de bash
history -c
sudo rm -f /root/.bash_history
rm -f ~/.bash_history

# Limpiar cache de paquetes
sudo apt clean        # Debian
# sudo dnf clean all  # AlmaLinux

# Limpiar archivos temporales
sudo rm -rf /tmp/* /var/tmp/*

# Apagar
sudo poweroff
```

### Por qué limpiar cada cosa

```
┌────────────────────────┬────────────────────────────────────────┐
│  Qué se limpia         │  Por qué                              │
├────────────────────────┼────────────────────────────────────────┤
│  /etc/machine-id       │  ID único de la máquina. Si dos VMs   │
│                        │  tienen el mismo, DHCP asigna la      │
│                        │  misma IP y systemd-journal se confunde│
├────────────────────────┼────────────────────────────────────────┤
│  ssh_host_*            │  Clave del host SSH. Si dos VMs        │
│                        │  comparten la misma key, SSH avisa     │
│                        │  de "host key changed" al reconectar  │
├────────────────────────┼────────────────────────────────────────┤
│  cloud-init data       │  Datos de instancia anterior. Sin      │
│                        │  limpiar, cloud-init no se re-ejecuta │
├────────────────────────┼────────────────────────────────────────┤
│  logs, bash_history    │  Reducir tamaño de la plantilla y     │
│                        │  no filtrar información de la setup    │
├────────────────────────┼────────────────────────────────────────┤
│  apt/dnf cache         │  Reducir tamaño de la imagen           │
└────────────────────────┴────────────────────────────────────────┘
```

### Paso 2: Alternativa con virt-sysprep

`virt-sysprep` (del paquete `guestfs-tools`) automatiza toda la limpieza
anterior en un solo comando, sin necesidad de entrar en la VM:

```bash
# La VM DEBE estar apagada
sudo virt-sysprep -d tpl-debian12-setup
```

```
[   0.0] Examining the guest ...
[   5.2] Performing "abrt-data" ...
[   5.2] Performing "backup-files" ...
[   5.3] Performing "bash-history" ...
[   5.3] Performing "blkid-tab" ...
[   5.3] Performing "crash-data" ...
[   5.3] Performing "cron-spool" ...
[   5.4] Performing "dhcp-client-state" ...
[   5.4] Performing "dhcp-server-state" ...
[   5.4] Performing "dovecot-data" ...
[   5.4] Performing "logfiles" ...
[   5.5] Performing "machine-id" ...
[   5.5] Performing "mail-spool" ...
[   5.5] Performing "net-hostname" ...
[   5.6] Performing "net-hwaddr" ...
[   5.6] Performing "pacct-log" ...
[   5.6] Performing "package-manager-cache" ...
[   5.6] Performing "pam-data" ...
[   5.6] Performing "puppet-data-log" ...
[   5.6] Performing "rh-subscription-manager" ...
[   5.6] Performing "rhn-systemid" ...
[   5.6] Performing "rpm-db" ...
[   5.7] Performing "samba-db-log" ...
[   5.7] Performing "script" ...
[   5.7] Performing "smolt-uuid" ...
[   5.7] Performing "ssh-hostkeys" ...
[   5.7] Performing "ssh-userdir" ...
[   5.7] Performing "sssd-db-log" ...
[   5.8] Performing "tmp-files" ...
[   5.8] Performing "udev-persistent-net" ...
[   5.8] Performing "utmp" ...
[   5.8] Performing "yum-uuid" ...
[   5.8] Performing "customize" ...
[   5.8] Setting a random seed
[   5.8] Setting the machine ID in /etc/machine-id to empty
```

`virt-sysprep` ejecuta docenas de operaciones de limpieza automáticamente.
Es la forma recomendada si tienes `guestfs-tools` instalado.

```bash
# Instalar si no lo tienes
sudo dnf install guestfs-tools    # Fedora
sudo apt install guestfs-tools    # Debian
```

### Paso 3: Eliminar la definición temporal de libvirt

La VM de setup ya no la necesitamos como VM. Solo queremos la imagen:

```bash
# Asegurarse de que está apagada
virsh list --all | grep tpl-debian12-setup
# shut off

# Eliminar la definición de la VM (sin borrar el disco)
virsh undefine tpl-debian12-setup

# El disco sigue en su sitio
ls -lh /var/lib/libvirt/images/tpl-debian12.qcow2
```

> **Importante**: usamos `virsh undefine` sin `--remove-all-storage`.
> Queremos borrar la VM de libvirt pero conservar la imagen qcow2.

### Paso 4: Aplanar el overlay (opcional pero recomendado)

La plantilla es actualmente un overlay de la imagen cloud. Para que sea
autónoma (sin dependencia del backing file):

```bash
# Convertir a imagen independiente
sudo qemu-img convert -f qcow2 -O qcow2 \
    tpl-debian12.qcow2 tpl-debian12-flat.qcow2

# Reemplazar
sudo mv tpl-debian12-flat.qcow2 tpl-debian12.qcow2
```

Ahora `tpl-debian12.qcow2` no tiene backing file. Es completamente
autónoma.

### Paso 5: Marcar como read-only

```bash
sudo chmod 444 /var/lib/libvirt/images/tpl-debian12.qcow2
sudo chmod 444 /var/lib/libvirt/images/tpl-alma9.qcow2

ls -l /var/lib/libvirt/images/tpl-*.qcow2
# -r--r--r-- 1 qemu qemu 1.8G ... tpl-debian12.qcow2
# -r--r--r-- 1 qemu qemu 2.1G ... tpl-alma9.qcow2
```

Las plantillas son ahora **inmutables**. Cualquier intento de arrancar una
VM directamente contra la plantilla fallará por permisos, lo cual es la
protección deseada.

### Limpiar archivos temporales

```bash
sudo rm -f /var/lib/libvirt/images/seed-tpl-*.iso
```

---

## Clonar desde plantilla: virt-clone

`virt-clone` crea una copia completa de una VM existente: clona el disco y
genera un nuevo XML con nombre y UUID diferentes.

Pero como eliminamos la definición de la VM, necesitamos primero crear una
VM dummy o clonar directamente la imagen:

### Clonar solo la imagen (sin virt-clone)

El método más directo para nuestro caso:

```bash
# Copia completa de la plantilla
sudo cp /var/lib/libvirt/images/tpl-debian12.qcow2 \
    /var/lib/libvirt/images/lab-ext4.qcow2
sudo chmod 644 /var/lib/libvirt/images/lab-ext4.qcow2

# Crear la VM con la copia
sudo virt-install \
    --name lab-ext4 \
    --ram 1024 \
    --vcpus 1 \
    --import \
    --disk path=/var/lib/libvirt/images/lab-ext4.qcow2 \
    --os-variant debian12 \
    --network network=default \
    --graphics none \
    --noautoconsole
```

### Usar virt-clone (si la VM template existe en libvirt)

Si hubieras mantenido la VM definida en libvirt (apagada), podrías usar:

```bash
sudo virt-clone \
    --original tpl-debian12 \
    --name lab-ext4 \
    --file /var/lib/libvirt/images/lab-ext4.qcow2
```

`virt-clone` copia el disco y genera automáticamente:
- Nuevo UUID
- Nuevo MAC address (evita conflictos de red)
- Nueva definición XML

> **Nota**: `virt-clone` siempre hace una **copia completa** del disco,
> lo cual tarda y ocupa espacio. Para labs efímeros, los overlays son
> superiores.

---

## Clonar desde plantilla: overlays

El método recomendado para labs: **overlays instantáneos** sobre la
plantilla read-only.

```bash
# Crear overlay (~0.1 segundos, ~200 KB)
sudo qemu-img create -f qcow2 \
    -b /var/lib/libvirt/images/tpl-debian12.qcow2 -F qcow2 \
    /var/lib/libvirt/images/lab-ext4.qcow2

# Crear la VM
sudo virt-install \
    --name lab-ext4 \
    --ram 1024 \
    --vcpus 1 \
    --import \
    --disk path=/var/lib/libvirt/images/lab-ext4.qcow2 \
    --os-variant debian12 \
    --network network=default \
    --graphics none \
    --noautoconsole
```

### Comparación de tiempos

```
virt-clone (copia completa):
  Crear disco: ~30 segundos (copiar 1.8 GB)
  Espacio:     1.8 GB por VM

Overlay:
  Crear disco: ~0.1 segundos
  Espacio:     ~200 KB inicial (crece con escrituras)
```

### Overlay con discos adicionales

Muchos labs necesitan discos extra (para practicar particionado, LVM,
RAID, etc.):

```bash
# Overlay del sistema operativo
sudo qemu-img create -f qcow2 \
    -b /var/lib/libvirt/images/tpl-debian12.qcow2 -F qcow2 \
    /var/lib/libvirt/images/lab-lvm-os.qcow2

# Discos adicionales vacíos para el lab
sudo qemu-img create -f qcow2 /var/lib/libvirt/images/lab-lvm-disk1.qcow2 5G
sudo qemu-img create -f qcow2 /var/lib/libvirt/images/lab-lvm-disk2.qcow2 5G
sudo qemu-img create -f qcow2 /var/lib/libvirt/images/lab-lvm-disk3.qcow2 5G

# VM con 1 disco de sistema + 3 discos de lab
sudo virt-install \
    --name lab-lvm \
    --ram 1024 \
    --vcpus 1 \
    --import \
    --disk path=/var/lib/libvirt/images/lab-lvm-os.qcow2 \
    --disk path=/var/lib/libvirt/images/lab-lvm-disk1.qcow2 \
    --disk path=/var/lib/libvirt/images/lab-lvm-disk2.qcow2 \
    --disk path=/var/lib/libvirt/images/lab-lvm-disk3.qcow2 \
    --os-variant debian12 \
    --network network=default \
    --graphics none \
    --noautoconsole
```

Dentro de la VM:

```bash
lsblk
# NAME   SIZE  TYPE
# vda     20G  disk    ← sistema operativo (overlay de plantilla)
# vdb      5G  disk    ← disco extra 1
# vdc      5G  disk    ← disco extra 2
# vdd      5G  disk    ← disco extra 3
```

### Destruir y recrear en segundos

```bash
# Destruir
sudo virsh destroy lab-lvm
sudo virsh undefine lab-lvm --remove-all-storage

# Recrear (repetir los comandos de creación)
# El overlay del SO es instantáneo
# Los discos vacíos son instantáneos
# Total: ~2 segundos
```

---

## Script de provisioning

Para automatizar la creación de VMs de lab, un script Bash que encapsule
todo el proceso:

### Script básico: una VM

```bash
#!/bin/bash
# create-lab-vm.sh — Crear una VM de lab a partir de la plantilla
# Uso: ./create-lab-vm.sh NOMBRE [NUM_DISCOS_EXTRA] [TAMAÑO_DISCO]

set -euo pipefail

# ── Parámetros ────────────────────────────────────────────────
VM_NAME="${1:?Uso: $0 NOMBRE [NUM_DISCOS] [SIZE]}"
NUM_DISCOS="${2:-0}"
DISCO_SIZE="${3:-5G}"
TEMPLATE="/var/lib/libvirt/images/tpl-debian12.qcow2"
POOL_DIR="/var/lib/libvirt/images"
OS_VARIANT="debian12"
RAM="1024"
VCPUS="1"

# ── Verificaciones ───────────────────────────────────────────
if virsh dominfo "$VM_NAME" &>/dev/null; then
    echo "ERROR: VM '$VM_NAME' ya existe" >&2
    exit 1
fi

if [[ ! -f "$TEMPLATE" ]]; then
    echo "ERROR: Plantilla no encontrada: $TEMPLATE" >&2
    exit 1
fi

# ── Crear overlay del sistema ────────────────────────────────
OS_DISK="$POOL_DIR/${VM_NAME}-os.qcow2"
qemu-img create -f qcow2 -b "$TEMPLATE" -F qcow2 "$OS_DISK"
echo "Creado overlay: $OS_DISK"

# ── Construir flags de disco ────────────────────────────────
DISK_FLAGS="--disk path=$OS_DISK"

for i in $(seq 1 "$NUM_DISCOS"); do
    EXTRA_DISK="$POOL_DIR/${VM_NAME}-disk${i}.qcow2"
    qemu-img create -f qcow2 "$EXTRA_DISK" "$DISCO_SIZE"
    DISK_FLAGS="$DISK_FLAGS --disk path=$EXTRA_DISK"
    echo "Creado disco extra: $EXTRA_DISK"
done

# ── Crear VM ─────────────────────────────────────────────────
virt-install \
    --name "$VM_NAME" \
    --ram "$RAM" \
    --vcpus "$VCPUS" \
    --import \
    $DISK_FLAGS \
    --os-variant "$OS_VARIANT" \
    --network network=default \
    --graphics none \
    --noautoconsole

echo ""
echo "VM '$VM_NAME' creada y arrancando."
echo "Conectar: virsh console $VM_NAME"
echo "Login: labuser / labpass123"
```

### Uso

```bash
chmod +x create-lab-vm.sh

# VM simple (solo disco de sistema)
sudo ./create-lab-vm.sh lab-fs

# VM con 3 discos extra de 5G
sudo ./create-lab-vm.sh lab-lvm 3

# VM con 4 discos extra de 2G
sudo ./create-lab-vm.sh lab-raid 4 2G
```

### Script de limpieza

```bash
#!/bin/bash
# destroy-lab-vm.sh — Destruir una VM de lab y sus discos
# Uso: ./destroy-lab-vm.sh NOMBRE

set -euo pipefail

VM_NAME="${1:?Uso: $0 NOMBRE}"

# Apagar si está corriendo
if virsh domstate "$VM_NAME" 2>/dev/null | grep -q running; then
    virsh destroy "$VM_NAME"
    echo "VM '$VM_NAME' apagada."
fi

# Eliminar definición y discos asociados
if virsh dominfo "$VM_NAME" &>/dev/null; then
    virsh undefine "$VM_NAME" --remove-all-storage
    echo "VM '$VM_NAME' eliminada con todos sus discos."
else
    echo "VM '$VM_NAME' no encontrada."
fi
```

### Script avanzado: múltiples VMs en paralelo

```bash
#!/bin/bash
# create-lab-env.sh — Crear un entorno de lab completo
# Uso: ./create-lab-env.sh PREFIJO NUM_VMS [NUM_DISCOS] [SIZE]

set -euo pipefail

PREFIX="${1:?Uso: $0 PREFIJO NUM_VMS [NUM_DISCOS] [SIZE]}"
NUM_VMS="${2:?Especificar número de VMs}"
NUM_DISCOS="${3:-0}"
DISCO_SIZE="${4:-5G}"

echo "Creando $NUM_VMS VMs con prefijo '$PREFIX'..."
echo ""

for i in $(seq 1 "$NUM_VMS"); do
    VM_NAME="${PREFIX}-$(printf '%02d' $i)"
    echo "── Creando $VM_NAME ──"
    sudo ./create-lab-vm.sh "$VM_NAME" "$NUM_DISCOS" "$DISCO_SIZE"
    echo ""
done

echo "═══════════════════════════════════════"
echo "Entorno listo. VMs creadas:"
virsh list --all | grep "$PREFIX"
echo ""
echo "Conectar: virsh console ${PREFIX}-01"
```

```bash
# Crear 3 VMs para lab de LVM, cada una con 3 discos de 5G
sudo ./create-lab-env.sh lab-lvm 3 3 5G

# Resultado:
# lab-lvm-01 (vda=SO, vdb/vdc/vdd=5G cada uno)
# lab-lvm-02 (vda=SO, vdb/vdc/vdd=5G cada uno)
# lab-lvm-03 (vda=SO, vdb/vdc/vdd=5G cada uno)
```

---

## Errores comunes

### 1. No deshabilitar cloud-init antes de sellar

```bash
# ❌ Plantilla con cloud-init activo
# Cada VM clonada tarda ~120s extra buscando datasource
# cloud-init puede cambiar configuraciones inesperadamente

# Síntoma: la VM tarda mucho en arrancar, mensajes en consola:
# "Waiting for cloud-init to complete..."
# "cloud-init[WARNING]: No datasource found"

# ✅ Deshabilitar antes de sellar
sudo touch /etc/cloud/cloud-init.disabled
```

### 2. No limpiar machine-id

```bash
# ❌ Dos VMs con el mismo machine-id
# Problemas:
#   - DHCP asigna la misma IP a ambas
#   - journalctl mezcla logs
#   - systemd-networkd confunde interfaces

# ✅ Truncar (no borrar) /etc/machine-id antes de sellar
sudo truncate -s 0 /etc/machine-id
# systemd lo regenerará al primer boot de cada clon
```

> **Nota**: `truncate -s 0` deja el archivo con tamaño 0. No uses `rm`
> porque algunos servicios fallan si el archivo no existe. Lo importante es
> que esté **vacío**, no ausente.

### 3. Olvidar chmod 444 en la plantilla

```bash
# ❌ Plantilla con permisos de escritura — riesgo de arrancar directamente
ls -l tpl-debian12.qcow2
# -rw-r--r-- ...  ← cualquier VM podría escribir aquí

# ✅ Read-only
sudo chmod 444 tpl-debian12.qcow2
# Intentar arrancar una VM contra este archivo dará error de permisos
```

### 4. No aplanar el overlay antes de sellar

```bash
# ❌ La plantilla depende de la imagen cloud original
qemu-img info tpl-debian12.qcow2 | grep backing
# backing file: debian-12-generic-amd64.qcow2

# Si borras la imagen cloud, la plantilla se corrompe
# Los overlays de la plantilla formarían cadenas de 3 niveles

# ✅ Aplanar antes de sellar
sudo qemu-img convert -f qcow2 -O qcow2 tpl-debian12.qcow2 tpl-flat.qcow2
sudo mv tpl-flat.qcow2 tpl-debian12.qcow2
```

### 5. Usar virt-clone para labs efímeros

```bash
# ❌ Copia completa para cada lab — lento y desperdicia espacio
sudo virt-clone --original template --name lab1 --file lab1.qcow2
# Tarda 30 segundos, ocupa 1.8 GB

# ✅ Overlay para labs efímeros
sudo qemu-img create -f qcow2 -b tpl-debian12.qcow2 -F qcow2 lab1.qcow2
# Tarda 0.1 segundos, ocupa 200 KB
```

Usa `virt-clone` solo cuando necesites una copia completamente independiente
(por ejemplo, para regalar a alguien). Para labs, siempre overlays.

---

## Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║  PLANTILLAS DE VMs — REFERENCIA RÁPIDA                              ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CREAR PLANTILLA:                                                  ║
║    1. Imagen cloud + cloud-init → VM configurada                   ║
║    2. Deshabilitar cloud-init:                                     ║
║       touch /etc/cloud/cloud-init.disabled                         ║
║       cloud-init clean                                             ║
║    3. Limpiar datos únicos:                                        ║
║       truncate -s 0 /etc/machine-id                                ║
║       rm /etc/ssh/ssh_host_*                                       ║
║       (o usar: virt-sysprep -d VM)                                 ║
║    4. Apagar y eliminar definición:                                ║
║       virsh undefine VM   (sin --remove-all-storage)               ║
║    5. Aplanar overlay:                                             ║
║       qemu-img convert -f qcow2 -O qcow2 tpl.qcow2 flat.qcow2    ║
║    6. Marcar read-only:                                            ║
║       chmod 444 tpl.qcow2                                         ║
║                                                                    ║
║  CLONAR PARA LAB (método overlay — recomendado):                   ║
║    qemu-img create -f qcow2 -b tpl.qcow2 -F qcow2 lab.qcow2      ║
║    virt-install --name lab --import --disk path=lab.qcow2 ...      ║
║                                                                    ║
║  DISCOS EXTRA PARA LAB:                                            ║
║    qemu-img create -f qcow2 extra.qcow2 5G                        ║
║    --disk path=lab.qcow2 --disk path=extra.qcow2                  ║
║                                                                    ║
║  DESTRUIR Y RECREAR:                                               ║
║    virsh destroy lab && virsh undefine lab --remove-all-storage     ║
║    (recrear overlay + virt-install)                                ║
║                                                                    ║
║  SCRIPTS DE PROVISIONING:                                          ║
║    ./create-lab-vm.sh NOMBRE [NUM_DISCOS] [SIZE]                   ║
║    ./destroy-lab-vm.sh NOMBRE                                      ║
║    ./create-lab-env.sh PREFIJO NUM_VMS [NUM_DISCOS] [SIZE]         ║
║                                                                    ║
║  DATOS QUE DEBEN SER ÚNICOS POR VM:                               ║
║    • /etc/machine-id (truncar, systemd regenera)                   ║
║    • /etc/ssh/ssh_host_* (borrar, sshd regenera)                   ║
║    • hostname (se puede cambiar con hostnamectl)                   ║
║    • cloud-init instance-id (limpiar con cloud-init clean)         ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Preparar y sellar una plantilla

**Objetivo**: recorrer el proceso completo de creación de plantilla.

> **Prerequisito**: imagen cloud de Debian 12 o AlmaLinux 9 descargada.

1. Sigue los pasos de la sección correspondiente a tu distro para crear
   la plantilla con cloud-init.

2. Dentro de la VM, verifica que todo funciona:
   ```bash
   cloud-init status                      # done
   sudo whoami                            # root (sin contraseña)
   dpkg -l vim curl tmux htop | grep ^ii  # 4 paquetes (Debian)
   # rpm -q vim-enhanced curl tmux htop   # AlmaLinux
   ```

3. Deshabilita cloud-init:
   ```bash
   sudo touch /etc/cloud/cloud-init.disabled
   sudo cloud-init clean
   ```

4. Limpia datos únicos:
   ```bash
   sudo truncate -s 0 /etc/machine-id
   sudo rm -f /var/lib/dbus/machine-id
   sudo rm -f /etc/ssh/ssh_host_*
   history -c && sudo rm -f /root/.bash_history ~/.bash_history
   sudo apt clean   # o dnf clean all
   ```

5. Apaga la VM:
   ```bash
   sudo poweroff
   ```

6. Desde el host, sella:
   ```bash
   sudo virsh undefine tpl-debian12-setup
   sudo qemu-img convert -f qcow2 -O qcow2 \
       /var/lib/libvirt/images/tpl-debian12.qcow2 \
       /var/lib/libvirt/images/tpl-debian12-flat.qcow2
   sudo mv /var/lib/libvirt/images/tpl-debian12-flat.qcow2 \
       /var/lib/libvirt/images/tpl-debian12.qcow2
   sudo chmod 444 /var/lib/libvirt/images/tpl-debian12.qcow2
   ```

7. Verifica:
   ```bash
   qemu-img info /var/lib/libvirt/images/tpl-debian12.qcow2 | grep backing
   # (sin output — no tiene backing file)
   ls -l /var/lib/libvirt/images/tpl-debian12.qcow2
   # -r--r--r-- ...
   ```

**Pregunta de reflexión**: ¿por qué es importante aplanar el overlay
antes de sellar la plantilla? ¿Qué pasaría si borras la imagen cloud
original sin haber aplanado?

---

### Ejercicio 2: Clonar con overlay y verificar unicidad

**Objetivo**: crear dos VMs desde la plantilla y verificar que son
independientes.

1. Crea dos overlays:
   ```bash
   sudo qemu-img create -f qcow2 \
       -b /var/lib/libvirt/images/tpl-debian12.qcow2 -F qcow2 \
       /var/lib/libvirt/images/test-a.qcow2
   sudo qemu-img create -f qcow2 \
       -b /var/lib/libvirt/images/tpl-debian12.qcow2 -F qcow2 \
       /var/lib/libvirt/images/test-b.qcow2
   ```

2. Crea dos VMs:
   ```bash
   for vm in test-a test-b; do
       sudo virt-install \
           --name $vm --ram 512 --vcpus 1 --import \
           --disk path=/var/lib/libvirt/images/${vm}.qcow2 \
           --os-variant debian12 --network network=default \
           --graphics none --noautoconsole
   done
   ```

3. Espera ~30 segundos y conéctate a cada VM. Verifica:
   ```bash
   # En test-a:
   cat /etc/machine-id
   ip addr show | grep "inet "
   cat /etc/ssh/ssh_host_ed25519_key.pub

   # En test-b:
   cat /etc/machine-id
   ip addr show | grep "inet "
   cat /etc/ssh/ssh_host_ed25519_key.pub
   ```

4. Compara: ¿tienen el mismo machine-id? ¿La misma IP? ¿Las mismas SSH
   host keys? (No deberían, gracias a la limpieza de la plantilla.)

5. Cambia el hostname en cada VM:
   ```bash
   # En test-a:
   sudo hostnamectl set-hostname test-a
   # En test-b:
   sudo hostnamectl set-hostname test-b
   ```

6. Compara los tamaños en disco:
   ```bash
   ls -lh /var/lib/libvirt/images/test-*.qcow2
   ls -lh /var/lib/libvirt/images/tpl-debian12.qcow2
   ```

7. Limpia:
   ```bash
   for vm in test-a test-b; do
       sudo virsh destroy $vm
       sudo virsh undefine $vm --remove-all-storage
   done
   ```

**Pregunta de reflexión**: si no hubieras truncado `/etc/machine-id` en la
plantilla, ¿qué problema concreto tendrías con DHCP al arrancar las dos
VMs en la misma red?

---

### Ejercicio 3: Script de provisioning para lab de LVM

**Objetivo**: crear y usar un script que provisione un entorno de lab
completo.

1. Guarda el script `create-lab-vm.sh` de la sección anterior en
   `~/scripts/`:
   ```bash
   mkdir -p ~/scripts
   # (copiar el script de la sección "Script básico: una VM")
   chmod +x ~/scripts/create-lab-vm.sh
   ```

2. Guarda también `destroy-lab-vm.sh`.

3. Crea una VM de lab para LVM con 3 discos extra:
   ```bash
   sudo ~/scripts/create-lab-vm.sh lab-lvm 3 5G
   ```

4. Verifica que la VM arrancó y tiene los discos:
   ```bash
   virsh console lab-lvm
   # Login: labuser / labpass123
   lsblk
   # vda  20G  (sistema)
   # vdb   5G  (disco extra)
   # vdc   5G  (disco extra)
   # vdd   5G  (disco extra)
   ```

5. Sal (`Ctrl+]`) y destruye:
   ```bash
   sudo ~/scripts/destroy-lab-vm.sh lab-lvm
   ```

6. Recrea inmediatamente:
   ```bash
   sudo ~/scripts/create-lab-vm.sh lab-lvm 3 5G
   ```

7. Mide el tiempo de recreación:
   ```bash
   time sudo ~/scripts/create-lab-vm.sh lab-lvm-timing 3 5G
   ```
   ¿Cuántos segundos tarda el proceso completo (crear overlay + discos +
   virt-install)?

8. Limpia todo:
   ```bash
   sudo ~/scripts/destroy-lab-vm.sh lab-lvm
   sudo ~/scripts/destroy-lab-vm.sh lab-lvm-timing
   ```

**Pregunta de reflexión**: el script usa `--noautoconsole` para que
virt-install devuelva el control inmediatamente. Si lanzaras 5 VMs en un
bucle sin este flag, ¿qué pasaría? ¿Podrías lanzar las 5 en paralelo?
