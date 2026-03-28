# Discos Virtuales en VMs

## Erratas detectadas en el material base

| Ubicacion | Error | Correccion |
|-----------|-------|------------|
| README.md:576 (Cache modes, `none`) | Dice "El filesystem del host puede cachear en su page cache", pero `cache=none` usa `O_DIRECT`, que **bypasea** el page cache del host por completo. No hay cache en ningun nivel: ni la interna de QEMU ni el page cache del host. Precisamente por eso es seguro ante cortes de luz. | Se corrige la descripcion: `cache=none` bypasea tanto la cache de QEMU como el page cache del host (O_DIRECT). El modo que si usa el page cache del host para lecturas es `writethrough`. |

---

## Indice

1. [Como QEMU presenta discos a la VM](#1-como-qemu-presenta-discos-a-la-vm)
2. [Buses de disco: virtio-blk vs virtio-scsi vs IDE/SATA](#2-buses-de-disco-virtio-blk-vs-virtio-scsi-vs-idesata)
3. [Anadir discos con virsh attach-disk](#3-anadir-discos-con-virsh-attach-disk)
4. [Quitar discos con virsh detach-disk](#4-quitar-discos-con-virsh-detach-disk)
5. [Anadir discos editando el XML de la VM](#5-anadir-discos-editando-el-xml-de-la-vm)
6. [Redimensionar discos: el flujo completo](#6-redimensionar-discos-el-flujo-completo)
7. [Backing files en produccion: linked clones con virsh](#7-backing-files-en-produccion-linked-clones-con-virsh)
8. [Cache modes: rendimiento vs seguridad](#8-cache-modes-rendimiento-vs-seguridad)
9. [Flujo completo de laboratorio](#9-flujo-completo-de-laboratorio)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Como QEMU presenta discos a la VM

Cuando defines una VM con libvirt, cada disco virtual tiene tres componentes:

```
 Definicion del disco en XML de la VM
┌─────────────────────────────────────────────────┐
│ 1. SOURCE (que archivo/dispositivo es el disco) │
│    /var/lib/libvirt/images/debian-lab.qcow2     │
│                                                 │
│ 2. TARGET (como lo ve la VM)                    │
│    dev='vda'  bus='virtio'                      │
│                                                 │
│ 3. DRIVER (como QEMU lo maneja)                 │
│    name='qemu'  type='qcow2'  cache='none'     │
└─────────────────────────────────────────────────┘
```

QEMU actua como traductor: intercepta las operaciones de I/O de la VM y las convierte en lecturas/escrituras sobre el archivo de imagen en el host:

```
 VM                          QEMU                         Host
┌──────────┐          ┌────────────────┐           ┌──────────────┐
│ App hace │          │                │           │              │
│ write()  │          │ Intercepta    │           │              │
│ en       │          │ la operacion  │           │ Escribe en   │
│ /dev/vda │─────────>│               │──────────>│ debian-lab   │
│          │ virtio   │ Traduce:      │ syscall   │ .qcow2       │
│          │ (rapido) │ sector VM ->  │ del host  │              │
│          │          │ offset archivo│           │              │
└──────────┘          └────────────────┘           └──────────────┘
```

El tipo de **bus** determina que driver usa la VM para comunicarse con QEMU, y esto afecta directamente al rendimiento y la nomenclatura de dispositivos dentro de la VM.

---

## 2. Buses de disco: virtio-blk vs virtio-scsi vs IDE/SATA

### virtio-blk: el recomendado

Paravirtualizacion — la VM sabe que esta virtualizada y usa un driver optimizado para comunicarse directamente con QEMU sin emular hardware real:

```bash
# Inside the VM
lsblk
# vda  252:0  0  20G  0 disk    <- prefix "vd" = virtio-blk
# vdb  252:16 0   5G  0 disk
```

| Caracteristica | Valor |
|---------------|-------|
| Dispositivo en VM | `/dev/vda`, `/dev/vdb`, ... |
| Major number | 252 |
| Rendimiento | Excelente (minimo overhead) |
| Hotplug | Si |
| Maximo de discos | ~28 (vda-vdz, mas con doble letra) |
| Requiere driver | virtio-blk en el kernel guest |
| TRIM/discard | Si |

### virtio-scsi: mas flexible

Tambien paravirtualizado, pero emula un controlador SCSI. Ofrece mas funcionalidades a costa de minima complejidad extra:

```bash
# Inside the VM
lsblk
# sda  8:0   0  20G  0 disk    <- prefix "sd" = SCSI
# sdb  8:16  0   5G  0 disk
```

| Caracteristica | Valor |
|---------------|-------|
| Dispositivo en VM | `/dev/sda`, `/dev/sdb`, ... |
| Major number | 8 |
| Rendimiento | Muy bueno (ligeramente menor que virtio-blk) |
| Hotplug | Si |
| Maximo de discos | Cientos (controlador SCSI sin limite practico) |
| Requiere driver | virtio-scsi en el kernel guest |
| Ventaja | Soporta comandos SCSI (reservations, persistent reserve) |

### IDE / SATA emulado: compatibilidad legacy

QEMU emula hardware IDE o SATA real. La VM no necesita drivers especiales — cualquier SO reconoce IDE:

```bash
# Inside the VM (IDE)
lsblk
# hda  3:0   0  20G  0 disk    <- prefix "hd" = IDE

# Inside the VM (emulated SATA)
lsblk
# sda  8:0   0  20G  0 disk    <- same prefix as SCSI
```

| Caracteristica | Valor |
|---------------|-------|
| Dispositivo en VM | `/dev/hda` (IDE) o `/dev/sda` (SATA) |
| Rendimiento | Bajo (emulacion completa de hardware) |
| Hotplug | No (IDE) / Limitado (SATA) |
| Maximo de discos | 4 (IDE), 6 (SATA ich9) |
| Requiere driver | No (driver generico en todo SO) |
| Uso | Windows legacy, SOs sin soporte virtio |

### Comparacion de rendimiento

```
 Operacion de I/O: camino que recorre una escritura

 virtio-blk:  VM -> driver virtio -> QEMU -> archivo host
              (1 capa de traduccion)

 virtio-scsi: VM -> driver virtio-scsi -> controlador SCSI virtual -> QEMU -> archivo
              (2 capas, pero ambas optimizadas)

 IDE:         VM -> driver IDE -> controlador IDE emulado -> traduccion registro
              a registro -> QEMU -> archivo
              (emulacion completa = lento)
```

### Cuando usar cada uno

```
 El SO guest tiene drivers virtio?
     Si -> Necesitas >28 discos o comandos SCSI?
          Si -> virtio-scsi
          No -> virtio-blk  <- recomendado para casi todo
     No -> IDE/SATA (Windows XP, SOs antiguos sin drivers virtio)
```

Para Linux moderno (cualquier kernel >= 2.6.25), **siempre virtio-blk**.

---

## 3. Anadir discos con virsh attach-disk

### Crear y adjuntar en un solo flujo

```bash
# 1. Create the image (on the host)
qemu-img create -f qcow2 /var/lib/libvirt/images/data-disk.qcow2 5G

# 2. Attach to the VM (hot, without shutting it down)
virsh attach-disk debian-lab \
  /var/lib/libvirt/images/data-disk.qcow2 vdb \
  --driver qemu --subdriver qcow2 --persistent
```

Desglose de los argumentos:

| Argumento | Significado |
|-----------|-------------|
| `debian-lab` | Nombre de la VM |
| `/var/lib/.../data-disk.qcow2` | Path a la imagen de disco |
| `vdb` | Target device en la VM (como se llamara dentro) |
| `--driver qemu` | QEMU maneja el disco (siempre para imagenes) |
| `--subdriver qcow2` | Formato de la imagen |
| `--persistent` | Guardar en el XML de la VM (sobrevive reinicio) |

### Sin --persistent: disco temporal

```bash
virsh attach-disk debian-lab \
  /var/lib/libvirt/images/temp-disk.qcow2 vdc \
  --driver qemu --subdriver qcow2
# Without --persistent: the disk is lost when the VM shuts down
```

Util para pruebas rapidas donde no quieres modificar la definicion de la VM.

### Adjuntar con bus especifico

```bash
# With virtio-scsi (appears as /dev/sdb in the VM)
virsh attach-disk debian-lab \
  /var/lib/libvirt/images/scsi-disk.qcow2 sdb \
  --driver qemu --subdriver qcow2 --targetbus scsi --persistent
```

### Adjuntar disco raw

```bash
qemu-img create -f raw /var/lib/libvirt/images/raw-disk.raw 5G

virsh attach-disk debian-lab \
  /var/lib/libvirt/images/raw-disk.raw vdb \
  --driver qemu --subdriver raw --persistent
```

### Adjuntar en modo solo lectura

```bash
virsh attach-disk debian-lab \
  /var/lib/libvirt/images/readonly-data.qcow2 vdc \
  --driver qemu --subdriver qcow2 --persistent --mode readonly
```

### Verificar desde ambos lados

```bash
# From the host: what disks does the VM have?
virsh domblklist debian-lab
#  Target   Source
# ─────────────────────────────────────────────────────
#  vda      /var/lib/libvirt/images/debian-lab.qcow2
#  vdb      /var/lib/libvirt/images/data-disk.qcow2

# More detail
virsh domblkinfo debian-lab vdb
#  Capacity:    5368709120        (5 GiB)
#  Allocation:  196608            (192 KiB - thin provisioning)
#  Physical:    196608

# Inside the VM
lsblk
# vda  252:0   0  20G  0 disk
# +--vda1 ...
# vdb  252:16  0   5G  0 disk    <- new disk, no partitions
```

---

## 4. Quitar discos con virsh detach-disk

```bash
# Detach disk by target
virsh detach-disk debian-lab vdb --persistent

# If the VM is running, the disk is hot-disconnected
# If --persistent, also removed from the XML
```

**Importante**: `detach-disk` desconecta el disco de la VM pero **no elimina el archivo de imagen**. El archivo qcow2/raw sigue en el host:

```bash
ls -lh /var/lib/libvirt/images/data-disk.qcow2
# -rw------- 1 qemu qemu 196K ...    <- still there
```

Para eliminar completamente:

```bash
virsh detach-disk debian-lab vdb --persistent
rm /var/lib/libvirt/images/data-disk.qcow2
```

### Precauciones antes de detach

```bash
# 1. Inside the VM: unmount the filesystem
sudo umount /data

# 2. Verify no processes are using it
sudo lsof +f -- /data

# 3. From the host: detach
virsh detach-disk debian-lab vdb --persistent
```

Si haces detach sin desmontar primero, la VM vera errores de I/O en los procesos que usaban ese disco.

---

## 5. Anadir discos editando el XML de la VM

Para configuraciones mas complejas (cache mode, io mode, serial number), editar el XML directamente da control total:

```bash
virsh edit debian-lab
```

### Bloque XML de un disco

```xml
<disk type='file' device='disk'>
  <driver name='qemu' type='qcow2' cache='none' io='native' discard='unmap'/>
  <source file='/var/lib/libvirt/images/data-disk.qcow2'/>
  <target dev='vdb' bus='virtio'/>
  <address type='pci' domain='0x0000' bus='0x07' slot='0x00' function='0x0'/>
</disk>
```

Cada elemento:

| Elemento | Atributo | Significado |
|----------|----------|-------------|
| `<disk>` | `type='file'` | La fuente es un archivo (vs `block` para dispositivo real) |
| | `device='disk'` | Es un disco (vs `cdrom`, `floppy`) |
| `<driver>` | `name='qemu'` | QEMU maneja el I/O |
| | `type='qcow2'` | Formato de la imagen |
| | `cache='none'` | Modo de cache (ver seccion 8) |
| | `io='native'` | Usa I/O nativo del kernel (vs `threads`) |
| | `discard='unmap'` | Soportar TRIM desde la VM |
| `<source>` | `file='...'` | Ruta al archivo de imagen |
| `<target>` | `dev='vdb'` | Nombre del dispositivo en la VM |
| | `bus='virtio'` | Bus a usar |
| `<address>` | (auto) | Direccion PCI — libvirt la asigna automaticamente |

### Anadir un disco via XML

```bash
virsh edit debian-lab
```

Buscar la seccion `<devices>` y anadir antes de `</devices>`:

```xml
    <disk type='file' device='disk'>
      <driver name='qemu' type='qcow2' cache='none' discard='unmap'/>
      <source file='/var/lib/libvirt/images/extra-disk.qcow2'/>
      <target dev='vdb' bus='virtio'/>
    </disk>
```

No incluyas `<address>` — libvirt la genera automaticamente al guardar.

Despues de guardar, reiniciar la VM para que tome el cambio (los cambios con `virsh edit` no aplican en caliente).

### Adjuntar un CD-ROM/ISO

```xml
<disk type='file' device='cdrom'>
  <driver name='qemu' type='raw'/>
  <source file='/var/lib/libvirt/images/debian-12.iso'/>
  <target dev='sda' bus='sata'/>
  <readonly/>
</disk>
```

---

## 6. Redimensionar discos: el flujo completo

Redimensionar un disco virtual tiene **tres pasos**: expandir la imagen, expandir la particion, expandir el filesystem. Omitir cualquiera deja espacio inutilizable.

### Paso 1: expandir la imagen (host)

```bash
# Shut down the VM (recommended)
virsh shutdown debian-lab

# Wait for shutdown
virsh list --all | grep debian-lab

# Expand the image by 5 GiB
qemu-img resize /var/lib/libvirt/images/debian-lab.qcow2 +5G
# Image resized.

# Verify
qemu-img info /var/lib/libvirt/images/debian-lab.qcow2 | grep "virtual size"
# virtual size: 25 GiB
```

Alternativa en caliente (VM corriendo):

```bash
virsh blockresize debian-lab /var/lib/libvirt/images/debian-lab.qcow2 25G
# Block device '/var/lib/libvirt/images/debian-lab.qcow2' is resized
```

**Importante**: NO uses `qemu-img resize` mientras la VM esta corriendo — puede corromper la imagen. Para resize en caliente, usa `virsh blockresize`.

### Paso 2: expandir la particion (dentro de la VM)

Arrancar la VM y verificar que el disco crecio:

```bash
lsblk
# vda    252:0    0   25G  0 disk       <- 25G (was 20G)
# +--vda1 252:1    0    1M  0 part
# +--vda2 252:2    0    1G  0 part /boot
# +--vda3 252:3    0   19G  0 part /      <- still 19G
```

El disco tiene 25G pero `vda3` sigue en 19G. Hay 5G libres al final del disco.

Expandir la particion con `growpart` (de cloud-utils-growpart):

```bash
# Install if not available
sudo apt install cloud-guest-utils    # Debian/Ubuntu
sudo dnf install cloud-utils-growpart # Fedora

# Expand partition 3 to use all available space
sudo growpart /dev/vda 3
# CHANGED: partition=3 start=2101248 old: size=39841759 end=41943007
#                                    new: size=50329567 end=52430815

lsblk
# +--vda3 252:3    0   24G  0 part /     <- now 24G
```

`growpart` modifica la tabla de particiones para que la particion ocupe el espacio libre contiguo. Es seguro en particiones montadas.

Alternativa sin growpart — con `fdisk` (mas manual):

```bash
# Manual method - growpart is safer
sudo fdisk /dev/vda
# d -> 3          (delete partition 3 - only the entry, not the data)
# n -> 3 -> ENTER -> ENTER  (recreate with same start, use all space)
# N               (don't remove filesystem signature)
# w               (write)
sudo partprobe /dev/vda
```

### Paso 3: expandir el filesystem (dentro de la VM)

La particion ahora tiene 24G pero el filesystem sigue viendo 19G:

```bash
df -h /
# Filesystem  Size  Used  Avail  Use%  Mounted on
# /dev/vda3    19G   5G    14G   27%   /
```

Expandir segun el tipo de filesystem:

```bash
# ext4: resize2fs (works while mounted)
sudo resize2fs /dev/vda3

# XFS: xfs_growfs (uses mountpoint, not device)
sudo xfs_growfs /

# Btrfs:
sudo btrfs filesystem resize max /
```

Verificar:

```bash
df -h /
# Filesystem  Size  Used  Avail  Use%  Mounted on
# /dev/vda3    24G   5G    19G   21%   /     <- now 24G
```

### Diagrama del flujo completo

```
 HOST                                    VM
┌───────────────────────────┐    ┌──────────────────────────┐
│                           │    │                          │
│ 1. virsh shutdown vm      │    │                          │
│         |                 │    │                          │
│ 2. qemu-img resize +5G   │    │                          │
│         |                 │    │                          │
│ 3. virsh start vm ────────┼───>│ 4. lsblk (ver 25G)      │
│                           │    │         |                │
│                           │    │ 5. growpart /dev/vda 3   │
│                           │    │    (expandir particion)  │
│                           │    │         |                │
│                           │    │ 6. resize2fs /dev/vda3   │
│                           │    │    (expandir filesystem) │
│                           │    │         |                │
│                           │    │ 7. df -h (verificar)     │
│                           │    │                          │
└───────────────────────────┘    └──────────────────────────┘
```

### Redimensionar un disco de datos (no el del SO)

Para discos de datos es mas sencillo porque puedes desmontar:

```bash
# Host: expand the image
qemu-img resize /var/lib/libvirt/images/data-disk.qcow2 +5G

# VM: expand partition and filesystem
sudo growpart /dev/vdb 1
sudo resize2fs /dev/vdb1    # ext4
# or
sudo xfs_growfs /data       # XFS

df -h /data
```

---

## 7. Backing files en produccion: linked clones con virsh

En T01 viste backing files con `qemu-img`. Aqui los usamos para crear VMs reales rapidamente a partir de una plantilla:

### Preparar la imagen base

```bash
# 1. Install and configure a "golden image" VM
# (install packages, configure SSH, etc.)

# 2. Shut down the VM
virsh shutdown debian-base

# 3. Mark the image as base (optional but recommended)
# Simply stop using this VM directly
```

### Crear VMs con linked clones

```bash
# 1. Create overlay for each VM
qemu-img create -f qcow2 \
  -b /var/lib/libvirt/images/debian-base.qcow2 -F qcow2 \
  /var/lib/libvirt/images/lab-vm1.qcow2

qemu-img create -f qcow2 \
  -b /var/lib/libvirt/images/debian-base.qcow2 -F qcow2 \
  /var/lib/libvirt/images/lab-vm2.qcow2

# 2. Clone the VM definition (not the disk)
virt-clone --original debian-base --name lab-vm1 \
  --file /var/lib/libvirt/images/lab-vm1.qcow2 \
  --preserve-data --check path_in_use=off

virt-clone --original debian-base --name lab-vm2 \
  --file /var/lib/libvirt/images/lab-vm2.qcow2 \
  --preserve-data --check path_in_use=off
```

`--preserve-data` le dice a virt-clone que no copie el disco — ya lo creamos como overlay. `--check path_in_use=off` permite usar un archivo que ya existe.

### Verificar la cadena

```bash
qemu-img info --backing-chain /var/lib/libvirt/images/lab-vm1.qcow2

# image: lab-vm1.qcow2
# backing file: debian-base.qcow2
# disk size: 196 KiB              <- almost nothing
#
# image: debian-base.qcow2
# disk size: 3.1 GiB              <- the shared base
```

### Alternativa rapida: virt-clone con --reflink

Si el filesystem del host soporta reflink (Btrfs, XFS con reflink):

```bash
virt-clone --original debian-base --name lab-vm1 --auto-clone
# On Btrfs/XFS with reflink, the copy is instant and doesn't duplicate data
```

---

## 8. Cache modes: rendimiento vs seguridad

El cache mode controla como QEMU maneja el cache de I/O entre la VM y el disco del host. Es una de las configuraciones mas impactantes en rendimiento.

### Los modos principales

```
 VM escribe datos -> donde se almacenan temporalmente?

 none:       VM -> QEMU -> directamente al disco host
             Usa O_DIRECT: bypasea tanto la cache de QEMU como el page cache
             del host. Los datos van directamente al almacenamiento.
             Seguro ante cortes de luz.

 writethrough: VM -> QEMU -> page cache del host (lectura) + disco (escritura)
               Las lecturas se cachean en el page cache del host,
               las escrituras van directo al disco.
               Seguro pero mas lento que writeback.

 writeback:  VM -> QEMU -> cache QEMU + page cache host -> disco (cuando convenga)
             QEMU cachea lecturas Y escrituras en RAM del host.
             Mas rapido pero datos pueden perderse en corte de luz.

 unsafe:     VM -> QEMU -> cache sin flush
             Ignora flush del guest. Maximo rendimiento.
             SOLO para pruebas — perdida de datos garantizada ante fallo.
```

### Comparacion

| Modo | Rendimiento | Seguridad de datos | Page cache host | Uso recomendado |
|------|:-----------:|:------------------:|:---------------:|-----------------|
| `none` | Bueno | Alta | Bypaseado (O_DIRECT) | **Default recomendado** para produccion |
| `writethrough` | Medio | Alta | Lecturas si, escrituras no | VMs que necesitan garantia total |
| `writeback` | Alto | Media | Si (lecturas y escrituras) | Labs, desarrollo |
| `unsafe` | Maximo | Ninguna | Si, sin flush | Instalacion de SOs, pruebas desechables |
| `directsync` | Bajo | Maxima | Bypaseado + O_SYNC | Bases de datos criticas |

### Configurar cache mode

```bash
# With virsh attach-disk
virsh attach-disk debian-lab \
  /var/lib/libvirt/images/data-disk.qcow2 vdb \
  --driver qemu --subdriver qcow2 --cache none --persistent

# In the XML (virsh edit)
# <driver name='qemu' type='qcow2' cache='none'/>
```

### io mode: threads vs native

Complementario al cache mode:

| io mode | Mecanismo | Cuando usar |
|---------|-----------|-------------|
| `threads` (default) | Pool de threads para I/O | cache=writeback, cache=writethrough |
| `native` | Linux AIO (io_uring / libaio) | cache=none (recomendado) |

```xml
<driver name='qemu' type='qcow2' cache='none' io='native'/>
```

`io=native` requiere que el cache mode use `O_DIRECT` (es decir, `cache=none` o `cache=directsync`). Con otros modos de cache, usar `io=threads`.

### discard='unmap': soporte TRIM

Permite que la VM envie comandos TRIM/discard al host, lo que libera clusters qcow2 cuando la VM borra archivos:

```xml
<driver name='qemu' type='qcow2' cache='none' discard='unmap'/>
```

Dentro de la VM:

```bash
# Free unused space
sudo fstrim -v /
# /: 2.1 GiB trimmed

# Or mount with discard option for automatic TRIM
# (in fstab inside the VM)
# UUID=...  /  ext4  defaults,discard  0  1
```

Con `discard='unmap'`, el archivo qcow2 en el host puede **reducir su tamano** cuando la VM borra archivos — algo que sin TRIM nunca ocurre.

---

## 9. Flujo completo de laboratorio

Este flujo resume todo C11 — desde crear una imagen hasta tener una VM con multiples discos completamente funcional:

### Escenario: VM con disco de sistema + disco de datos

```bash
# ===============================================
# ON THE HOST
# ===============================================

# 1. Create image for data disk
qemu-img create -f qcow2 /var/lib/libvirt/images/debian-data.qcow2 10G

# 2. Attach to the VM
virsh attach-disk debian-lab \
  /var/lib/libvirt/images/debian-data.qcow2 vdb \
  --driver qemu --subdriver qcow2 --persistent

# 3. Verify
virsh domblklist debian-lab
# Target   Source
# vda      /var/lib/libvirt/images/debian-lab.qcow2
# vdb      /var/lib/libvirt/images/debian-data.qcow2


# ===============================================
# INSIDE THE VM
# ===============================================

# 4. Verify new disk
lsblk
# vdb  252:16  0  10G  0 disk

# 5. Create GPT partition table
sudo fdisk /dev/vdb
# g -> n -> ENTER -> ENTER -> ENTER -> w

# 6. Create filesystem
sudo mkfs.ext4 -L "DATA" /dev/vdb1

# 7. Create mount point and mount
sudo mkdir -p /data
sudo mount /dev/vdb1 /data

# 8. Configure persistent mount
sudo blkid /dev/vdb1
# UUID="xxxx-yyyy-zzzz" TYPE="ext4"

echo 'UUID=xxxx-yyyy-zzzz  /data  ext4  defaults,noatime,nofail  0  2' \
  | sudo tee -a /etc/fstab

# 9. Verify fstab
sudo umount /data
sudo mount -a
df -h /data
# /dev/vdb1  9.8G  24K  9.3G  1%  /data

# 10. Create test data
echo "Lab operativo" | sudo tee /data/status.txt


# ===============================================
# BACK ON THE HOST - verify
# ===============================================

# 11. How much space does it really use?
qemu-img info /var/lib/libvirt/images/debian-data.qcow2
# virtual size: 10 GiB
# disk size: 33.1 MiB     <- thin provisioning in action

# 12. Snapshot before experimenting
virsh snapshot-create-as debian-lab snap-lab-ready \
  --description "Lab configured with data disk"
```

### Escenario: expandir el disco de datos despues

```bash
# HOST: expand image
virsh shutdown debian-lab
qemu-img resize /var/lib/libvirt/images/debian-data.qcow2 +5G
virsh start debian-lab

# VM: expand partition and filesystem
sudo growpart /dev/vdb 1
sudo resize2fs /dev/vdb1
df -h /data
# /dev/vdb1  14.7G  24K  13.9G  1%  /data  <- 10G -> 15G
```

---

## 10. Errores comunes

### Error 1: attach-disk sin --subdriver

```bash
virsh attach-disk debian-lab \
  /var/lib/libvirt/images/disk.qcow2 vdb \
  --driver qemu --persistent
# The VM may not boot or see corrupt data
```

**Por que falla**: sin `--subdriver qcow2`, QEMU puede intentar abrir la imagen como raw, interpretando los metadatos qcow2 como datos del disco.

**Solucion**: siempre especificar el formato:

```bash
virsh attach-disk debian-lab \
  /var/lib/libvirt/images/disk.qcow2 vdb \
  --driver qemu --subdriver qcow2 --persistent
```

### Error 2: expandir la imagen pero olvidar growpart y resize2fs

```bash
qemu-img resize disk.qcow2 +10G
# "Done! The disk now has 10G more"
# Inside the VM: df -h / -> still shows original size
```

**Por que pasa**: `qemu-img resize` solo cambia el disco virtual. La tabla de particiones y el filesystem no se enteran.

**Solucion**: el flujo tiene 3 pasos obligatorios:

```bash
qemu-img resize disk.qcow2 +10G     # 1. image
sudo growpart /dev/vda 3              # 2. partition
sudo resize2fs /dev/vda3              # 3. filesystem
```

### Error 3: detach sin desmontar primero

```bash
# Inside the VM: /data is mounted on /dev/vdb1
# On the host:
virsh detach-disk debian-lab vdb --persistent
# The VM starts showing I/O errors
```

**Por que pasa**: el kernel de la VM intenta leer/escribir un dispositivo que ya no existe.

**Solucion**: siempre desmontar dentro de la VM primero:

```bash
# VM:
sudo umount /data
# Host:
virsh detach-disk debian-lab vdb --persistent
```

### Error 4: usar cache=writeback y perder datos

```bash
# Configure aggressive cache for "better performance"
# <driver name='qemu' type='qcow2' cache='writeback'/>

# Power cut or kill -9 of the QEMU process
# -> cached data lost, filesystem potentially corrupt
```

**Por que pasa**: `writeback` mantiene escrituras en RAM del host antes de enviarlas al disco. Un fallo pierde esas escrituras.

**Solucion**: usar `cache='none'` para produccion y labs con datos importantes. Reservar `writeback` para VMs desechables donde el rendimiento importa mas que la integridad.

### Error 5: permisos incorrectos en la imagen

```bash
qemu-img create -f qcow2 /var/lib/libvirt/images/new-disk.qcow2 5G
virsh attach-disk debian-lab \
  /var/lib/libvirt/images/new-disk.qcow2 vdb \
  --driver qemu --subdriver qcow2 --persistent
# error: Cannot access storage file: Permission denied
```

**Por que pasa**: QEMU corre como usuario `qemu` (o `libvirt-qemu`). El archivo recien creado pertenece a tu usuario.

**Solucion**:

```bash
sudo chown qemu:qemu /var/lib/libvirt/images/new-disk.qcow2
# or on Debian/Ubuntu:
sudo chown libvirt-qemu:kvm /var/lib/libvirt/images/new-disk.qcow2

# Alternative: create as root directly in the images directory
sudo qemu-img create -f qcow2 /var/lib/libvirt/images/new-disk.qcow2 5G
```

---

## 11. Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│             DISCOS VIRTUALES EN VMs — REFERENCIA                 │
├──────────────────────────────────────────────────────────────────┤
│ BUSES                                                            │
│   virtio-blk    /dev/vda,vdb...   Recomendado (paravirtualizado)│
│   virtio-scsi   /dev/sda,sdb...   Muchos discos, comandos SCSI │
│   IDE           /dev/hda,hdb...   Legacy, sin hotplug           │
├──────────────────────────────────────────────────────────────────┤
│ ADJUNTAR DISCOS                                                  │
│   qemu-img create -f qcow2       Crear imagen                   │
│     img.qcow2 5G                                                 │
│   virsh attach-disk <vm>          Adjuntar a VM                  │
│     img.qcow2 vdb --driver qemu                                 │
│     --subdriver qcow2 --persistent                               │
│   virsh domblklist <vm>           Listar discos de la VM        │
│   virsh domblkinfo <vm> vdb       Info detallada de un disco    │
├──────────────────────────────────────────────────────────────────┤
│ QUITAR DISCOS                                                    │
│   (VM) umount /data               1. Desmontar en la VM        │
│   virsh detach-disk <vm> vdb      2. Desconectar                │
│     --persistent                                                 │
│   rm img.qcow2                    3. Borrar archivo (opcional)  │
├──────────────────────────────────────────────────────────────────┤
│ REDIMENSIONAR (3 PASOS)                                          │
│   qemu-img resize img.qcow2 +5G  1. Expandir imagen (host)     │
│   growpart /dev/vda 3             2. Expandir particion (VM)    │
│   resize2fs /dev/vda3       (ext4) 3. Expandir filesystem (VM)  │
│   xfs_growfs /              (xfs)                                │
│   btrfs filesystem resize   (btrfs)                              │
│     max /mountpoint                                              │
├──────────────────────────────────────────────────────────────────┤
│ RESIZE EN CALIENTE                                               │
│   virsh blockresize <vm>          (NO usar qemu-img resize      │
│     img.qcow2 25G                 con VM corriendo)             │
├──────────────────────────────────────────────────────────────────┤
│ LINKED CLONES                                                    │
│   qemu-img create -f qcow2       Crear overlay                  │
│     -b base.qcow2 -F qcow2                                      │
│     overlay.qcow2                                                │
│   virt-clone --original base-vm   Clonar definicion de VM       │
│     --name clone --file overlay                                  │
│     --preserve-data                                              │
├──────────────────────────────────────────────────────────────────┤
│ CACHE MODES                                                      │
│   none          O_DIRECT, seguro, buen rendimiento (recomendado)│
│   writethrough  Page cache lectura, seguro, mas lento           │
│   writeback     Todo cacheado, rapido, riesgo ante fallos       │
│   unsafe        Sin flush, maximo rendimiento, solo pruebas     │
│   directsync    O_DIRECT + O_SYNC, maxima seguridad             │
├──────────────────────────────────────────────────────────────────┤
│ XML CLAVE                                                        │
│   <driver name='qemu' type='qcow2'                              │
│     cache='none' io='native'                                     │
│     discard='unmap'/>                                            │
│   <target dev='vdb' bus='virtio'/>                               │
└──────────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: Identificar buses de disco

Dentro de una VM con Linux, ejecuta `lsblk` y observa los nombres de dispositivos.

**Prediccion**: si ves `/dev/vda`, el bus es ____. Si ves `/dev/sda`, el bus podria ser ____ o ____. Si ves `/dev/hda`, el bus es ____.

<details><summary>Respuesta</summary>

- `/dev/vda` → **virtio-blk** (paravirtualizado, major 252)
- `/dev/sda` → **virtio-scsi** o **SATA emulado** (ambos usan el prefijo `sd`, major 8). Para distinguirlos puedes ver el XML de la VM (`virsh dumpxml`) y buscar `bus='virtio'` vs `bus='sata'`.
- `/dev/hda` → **IDE** (legacy, major 3)

En el ecosistema QEMU/KVM moderno, virtio-blk (`vd*`) es el estandar. Solo verias `hd*` en configuraciones legacy o SOs sin drivers virtio.

</details>

---

### Ejercicio 2: Adjuntar y verificar un disco

En el host, crea una imagen y adjuntala a una VM:

```bash
qemu-img create -f qcow2 /tmp/exercise-disk.qcow2 3G
```

1. Adjunta con `virsh attach-disk` (usa `--driver qemu --subdriver qcow2 --persistent`).
2. Ejecuta `virsh domblklist <vm>`.

**Prediccion**: cuantos discos aparecen en la lista y cual es el target del nuevo?

<details><summary>Respuesta</summary>

Aparecen **al menos 2**: el disco del sistema (probablemente `vda`) y el nuevo (`vdb`). Si la VM ya tenia otros discos, aparecen tambien.

El target `vdb` es el segundo disco virtio-blk. Si hubieras usado `--targetbus scsi`, apareceria como `sdb`.

</details>

3. Dentro de la VM, ejecuta `lsblk`.

**Prediccion**: el nuevo disco aparecera como ____ con tamano ____ y tipo ____

<details><summary>Respuesta</summary>

- Nombre: **vdb** (segundo disco virtio)
- Tamano: **3G**
- Tipo: **disk** (sin particiones aun)

No tiene particiones ni filesystem — es un disco "en blanco" que necesita fdisk + mkfs para ser utilizable.

</details>

4. Limpia: `virsh detach-disk <vm> vdb --persistent && rm /tmp/exercise-disk.qcow2`

---

### Ejercicio 3: domblkinfo y thin provisioning

Con un disco qcow2 adjuntado a una VM, ejecuta:

```bash
virsh domblkinfo <vm> vdb
```

**Prediccion**: si el disco es de 5 GiB y la VM escribio ~200 MiB, los tres campos seran:
- Capacity: ____
- Allocation: ____
- Physical: ____

<details><summary>Respuesta</summary>

- **Capacity**: 5368709120 (5 GiB en bytes — tamano virtual)
- **Allocation**: ~200 MiB en bytes (~209715200) — espacio realmente usado por clusters qcow2
- **Physical**: similar a Allocation — tamano fisico del archivo en el host

Capacity es lo que la VM ve. Allocation/Physical reflejan el thin provisioning: solo se ocupa espacio para los datos escritos.

</details>

---

### Ejercicio 4: Flujo completo de particion, formato y montaje

Con un disco `vdb` adjuntado y vacio, haz dentro de la VM:

```bash
sudo fdisk /dev/vdb    # g -> n -> ENTER -> ENTER -> ENTER -> w
sudo mkfs.ext4 -L "LAB" /dev/vdb1
sudo mkdir -p /lab
sudo mount /dev/vdb1 /lab
```

1. Ejecuta `lsblk -f /dev/vdb`.

**Prediccion**: que columnas relevantes mostrara para vdb1?

<details><summary>Respuesta</summary>

- **FSTYPE**: ext4
- **LABEL**: LAB
- **UUID**: un UUID generado por mkfs (ej: a1b2c3d4-...)
- **MOUNTPOINTS**: /lab

`lsblk -f` muestra la relacion entre dispositivo, filesystem, UUID y punto de montaje.

</details>

2. Configura en fstab usando UUID y `nofail`:
   ```bash
   sudo blkid /dev/vdb1    # get UUID
   echo 'UUID=<uuid>  /lab  ext4  defaults,noatime,nofail  0  2' | sudo tee -a /etc/fstab
   ```

**Prediccion**: por que usamos `nofail` y no solo `defaults`?

<details><summary>Respuesta</summary>

**`nofail`** permite que el sistema arranque normalmente incluso si el disco no esta presente. Sin `nofail`, si alguien hace `virsh detach-disk` sin editar fstab, la VM entraria en **emergency mode** al reiniciar porque no puede montar un dispositivo listado en fstab. Con `nofail`, simplemente lo omite y continua el boot.

Para discos de datos removibles (no el disco del SO), `nofail` es una practica defensiva esencial.

</details>

---

### Ejercicio 5: XML de disco — leer y entender

Ejecuta `virsh dumpxml <vm>` y busca el bloque `<disk>` del disco principal.

**Prediccion**: que atributos esperas ver en `<driver>`? Nombra al menos 3.

<details><summary>Respuesta</summary>

- `name='qemu'` — QEMU es el encargado del I/O
- `type='qcow2'` (o `raw`) — formato de la imagen
- `cache='...'` — modo de cache (none, writeback, writethrough, etc.)

Opcionalmente puede tener:
- `io='native'` o `io='threads'` — mecanismo de I/O
- `discard='unmap'` — soporte TRIM
- `error_policy='...'` — que hacer ante errores de I/O

El bloque `<target>` complementa con `dev='vda'` y `bus='virtio'`.

</details>

---

### Ejercicio 6: Redimensionar disco — los 3 pasos

1. Crea un disco de 2 GiB, adjuntalo, y dentro de la VM haz fdisk + mkfs.ext4 + mount.
2. Crea un archivo de 500 MiB: `sudo dd if=/dev/urandom of=/lab/bigfile bs=1M count=500`
3. Verifica con `df -h /lab`.

**Prediccion**: con un disco de 2 GiB, despues de escribir 500 MiB, `df -h` mostrara aproximadamente ____ disponibles.

<details><summary>Respuesta</summary>

Aproximadamente **1.3-1.4 GiB** disponibles. De los 2 GiB del disco: ~1.9 GiB es el tamano del filesystem (el resto es overhead de particion), ext4 reserva 5% para root (~95 MiB), y los 500 MiB estan ocupados por el archivo.

</details>

4. En el host: `qemu-img resize <imagen> 5G` (VM apagada) o `virsh blockresize` (VM corriendo).
5. Dentro de la VM: `sudo growpart /dev/vdb 1`

**Prediccion**: despues de growpart pero ANTES de resize2fs, `df -h` mostrara ____ y `lsblk` mostrara ____

<details><summary>Respuesta</summary>

- `df -h /lab`: **sigue mostrando ~1.9 GiB** de tamano total — el filesystem no sabe que la particion crecio
- `lsblk`: vdb1 muestra **~5G** — la particion ya se expandio

Este es el paso intermedio donde la particion es grande pero el filesystem no lo sabe aun. `resize2fs` cierra la brecha.

</details>

6. `sudo resize2fs /dev/vdb1` y verifica con `df -h`.

---

### Ejercicio 7: Cache mode — inspeccionar configuracion actual

Ejecuta `virsh dumpxml <vm> | grep -A2 "driver.*qemu"` para ver la configuracion de cache de tus discos.

**Prediccion**: si no especificaste cache mode al crear la VM, el valor por defecto sera ____

<details><summary>Respuesta</summary>

Depende de la version de libvirt y la configuracion, pero el default tipico es **no especificado** (lo que se traduce en `writethrough` historicamente, o `none` en configuraciones modernas de libvirt).

Si no aparece el atributo `cache` en el XML, libvirt usa su valor por defecto. Para verificar el modo efectivo puedes mirar los argumentos del proceso QEMU: `ps aux | grep qemu | grep cache`.

Para produccion, es mejor especificarlo explicitamente con `cache='none'`.

</details>

---

### Ejercicio 8: discard/TRIM en accion

Adjunta un disco qcow2 de 2 GiB con `discard='unmap'` en el XML:

```xml
<driver name='qemu' type='qcow2' cache='none' discard='unmap'/>
```

1. Dentro de la VM: mkfs.ext4, mount, escribe 500 MiB de datos.
2. En el host: `qemu-img info <imagen>` — anota el `disk size`.
3. Dentro de la VM: borra los datos (`rm`).
4. Ejecuta `sudo fstrim -v /mountpoint`.

**Prediccion**: despues de `fstrim`, el `disk size` en el host sera ____ comparado con antes de fstrim.

<details><summary>Respuesta</summary>

El `disk size` sera **significativamente menor** — cercano a lo que era antes de escribir los 500 MiB.

`fstrim` envia comandos DISCARD al disco virtual. Con `discard='unmap'`, QEMU recibe estos comandos y libera los clusters qcow2 correspondientes, reduciendo el tamano fisico del archivo.

Sin `discard='unmap'`, el `rm` dentro de la VM no cambia el `disk size` en absoluto — los clusters siguen asignados aunque el filesystem los marque como libres.

</details>

---

### Ejercicio 9: Linked clone con virt-clone

Si tienes una VM base:

1. Apagala.
2. Crea un overlay:
   ```bash
   qemu-img create -f qcow2 -b /var/lib/libvirt/images/<base>.qcow2 \
     -F qcow2 /var/lib/libvirt/images/clone-test.qcow2
   ```
3. Clona la definicion:
   ```bash
   virt-clone --original <base-vm> --name clone-test \
     --file /var/lib/libvirt/images/clone-test.qcow2 \
     --preserve-data --check path_in_use=off
   ```

**Prediccion**: el overlay recien creado ocupa ____ segun `qemu-img info`.

<details><summary>Respuesta</summary>

**~196 KiB** (solo header y tablas L1/L2, sin datos propios). Todo el contenido se lee del backing file.

Al arrancar el clon y hacer cambios, el overlay crecera conforme la VM escriba bloques. Pero los bloques no modificados nunca se copian — se leen directamente del backing file.

</details>

4. Arranca el clon y cambia el hostname.
5. Compara `qemu-img info` del overlay antes y despues.

**Prediccion**: el `disk size` del overlay crecio? Cuanto aproximadamente?

<details><summary>Respuesta</summary>

**Si**, crecio unos pocos MiB. Cambiar el hostname modifica `/etc/hostname` y potencialmente `/etc/hosts`, que son archivos pequenos. Pero el boot de la VM escribe logs, actualiza timestamps, etc., lo que genera mas escrituras. Tipicamente el overlay crece unos **5-30 MiB** despues del primer arranque.

</details>

---

### Ejercicio 10: Error de permisos y solucion

Crea una imagen como tu usuario normal:

```bash
qemu-img create -f qcow2 /var/lib/libvirt/images/perm-test.qcow2 1G
```

1. Intenta adjuntar con `virsh attach-disk`.

**Prediccion**: el resultado sera ____

<details><summary>Respuesta</summary>

**Error**: `Cannot access storage file '/var/lib/libvirt/images/perm-test.qcow2': Permission denied`

QEMU corre como usuario `qemu` (RHEL/Fedora) o `libvirt-qemu` (Debian/Ubuntu), y el archivo fue creado con permisos de tu usuario. QEMU no puede leerlo.

</details>

2. Verifica los permisos: `ls -la /var/lib/libvirt/images/perm-test.qcow2`

**Prediccion**: el propietario sera ____ y deberia ser ____

<details><summary>Respuesta</summary>

- **Es**: `<tu-usuario>:<tu-grupo>` (ej: `zavden:zavden`)
- **Deberia ser**: `qemu:qemu` (RHEL/Fedora) o `libvirt-qemu:kvm` (Debian/Ubuntu)

Solucion:
```bash
sudo chown qemu:qemu /var/lib/libvirt/images/perm-test.qcow2  # RHEL/Fedora
# or
sudo chown libvirt-qemu:kvm /var/lib/libvirt/images/perm-test.qcow2  # Debian/Ubuntu
```

Alternativa preventiva: crear la imagen con `sudo`:
```bash
sudo qemu-img create -f qcow2 /var/lib/libvirt/images/perm-test.qcow2 1G
```

</details>

3. Corrige y reintenta el attach.
4. Limpia: `virsh detach-disk <vm> vdb --persistent && sudo rm /var/lib/libvirt/images/perm-test.qcow2`
