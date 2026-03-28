# Discos Virtuales en VMs

## Índice

1. [Cómo QEMU presenta discos a la VM](#1-cómo-qemu-presenta-discos-a-la-vm)
2. [Buses de disco: virtio-blk vs virtio-scsi vs IDE/SATA](#2-buses-de-disco-virtio-blk-vs-virtio-scsi-vs-idesata)
3. [Añadir discos con virsh attach-disk](#3-añadir-discos-con-virsh-attach-disk)
4. [Quitar discos con virsh detach-disk](#4-quitar-discos-con-virsh-detach-disk)
5. [Añadir discos editando el XML de la VM](#5-añadir-discos-editando-el-xml-de-la-vm)
6. [Redimensionar discos: el flujo completo](#6-redimensionar-discos-el-flujo-completo)
7. [Backing files en producción: linked clones con virsh](#7-backing-files-en-producción-linked-clones-con-virsh)
8. [Cache modes: rendimiento vs seguridad](#8-cache-modes-rendimiento-vs-seguridad)
9. [Flujo completo de laboratorio](#9-flujo-completo-de-laboratorio)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Cómo QEMU presenta discos a la VM

Cuando defines una VM con libvirt, cada disco virtual tiene tres componentes:

```
 Definición del disco en XML de la VM
┌─────────────────────────────────────────────────┐
│ 1. SOURCE (qué archivo/dispositivo es el disco) │
│    /var/lib/libvirt/images/debian-lab.qcow2     │
│                                                 │
│ 2. TARGET (cómo lo ve la VM)                    │
│    dev='vda'  bus='virtio'                      │
│                                                 │
│ 3. DRIVER (cómo QEMU lo maneja)                 │
│    name='qemu'  type='qcow2'  cache='none'     │
└─────────────────────────────────────────────────┘
```

QEMU actúa como traductor: intercepta las operaciones de I/O de la VM y las convierte en lecturas/escrituras sobre el archivo de imagen en el host:

```
 VM                          QEMU                         Host
┌──────────┐          ┌────────────────┐           ┌──────────────┐
│ App hace │          │                │           │              │
│ write()  │          │ Intercepta    │           │              │
│ en       │          │ la operación  │           │ Escribe en   │
│ /dev/vda │─────────►│               │──────────►│ debian-lab   │
│          │ virtio   │ Traduce:      │ syscall   │ .qcow2       │
│          │ (rápido) │ sector VM →   │ del host  │              │
│          │          │ offset archivo│           │              │
└──────────┘          └────────────────┘           └──────────────┘
```

El tipo de **bus** determina qué driver usa la VM para comunicarse con QEMU, y esto afecta directamente al rendimiento y la nomenclatura de dispositivos dentro de la VM.

---

## 2. Buses de disco: virtio-blk vs virtio-scsi vs IDE/SATA

### virtio-blk: el recomendado

Paravirtualización — la VM sabe que está virtualizada y usa un driver optimizado para comunicarse directamente con QEMU sin emular hardware real:

```bash
# Dentro de la VM
lsblk
# vda  252:0  0  20G  0 disk    ← prefijo "vd" = virtio-blk
# vdb  252:16 0   5G  0 disk
```

| Característica | Valor |
|---------------|-------|
| Dispositivo en VM | `/dev/vda`, `/dev/vdb`, ... |
| Major number | 252 |
| Rendimiento | Excelente (mínimo overhead) |
| Hotplug | Sí |
| Máximo de discos | ~28 (vda–vdz, más con doble letra) |
| Requiere driver | virtio-blk en el kernel guest |
| TRIM/discard | Sí |

### virtio-scsi: más flexible

También paravirtualizado, pero emula un controlador SCSI. Ofrece más funcionalidades a costa de mínima complejidad extra:

```bash
# Dentro de la VM
lsblk
# sda  8:0   0  20G  0 disk    ← prefijo "sd" = SCSI
# sdb  8:16  0   5G  0 disk
```

| Característica | Valor |
|---------------|-------|
| Dispositivo en VM | `/dev/sda`, `/dev/sdb`, ... |
| Major number | 8 |
| Rendimiento | Muy bueno (ligeramente menor que virtio-blk) |
| Hotplug | Sí |
| Máximo de discos | Cientos (controlador SCSI sin límite práctico) |
| Requiere driver | virtio-scsi en el kernel guest |
| Ventaja | Soporta comandos SCSI (reservations, persistent reserve) |

### IDE / SATA emulado: compatibilidad legacy

QEMU emula hardware IDE o SATA real. La VM no necesita drivers especiales — cualquier SO reconoce IDE:

```bash
# Dentro de la VM (IDE)
lsblk
# hda  3:0   0  20G  0 disk    ← prefijo "hd" = IDE

# Dentro de la VM (SATA emulado)
lsblk
# sda  8:0   0  20G  0 disk    ← mismo prefijo que SCSI
```

| Característica | Valor |
|---------------|-------|
| Dispositivo en VM | `/dev/hda` (IDE) o `/dev/sda` (SATA) |
| Rendimiento | Bajo (emulación completa de hardware) |
| Hotplug | No (IDE) / Limitado (SATA) |
| Máximo de discos | 4 (IDE), 6 (SATA ich9) |
| Requiere driver | No (driver genérico en todo SO) |
| Uso | Windows legacy, SOs sin soporte virtio |

### Comparación de rendimiento

```
 Operación de I/O: camino que recorre una escritura

 virtio-blk:  VM → driver virtio → QEMU → archivo host
              (1 capa de traducción)

 virtio-scsi: VM → driver virtio-scsi → controlador SCSI virtual → QEMU → archivo
              (2 capas, pero ambas optimizadas)

 IDE:         VM → driver IDE → controlador IDE emulado → traducción registro
              a registro → QEMU → archivo
              (emulación completa = lento)
```

### Cuándo usar cada uno

```
 ¿El SO guest tiene drivers virtio?
     Sí → ¿Necesitas >28 discos o comandos SCSI?
          Sí → virtio-scsi
          No → virtio-blk  ← recomendado para casi todo
     No → IDE/SATA (Windows XP, SOs antiguos sin drivers virtio)
```

Para Linux moderno (cualquier kernel ≥ 2.6.25), **siempre virtio-blk**.

---

## 3. Añadir discos con virsh attach-disk

### Crear y adjuntar en un solo flujo

```bash
# 1. Crear la imagen (en el host)
qemu-img create -f qcow2 /var/lib/libvirt/images/data-disk.qcow2 5G

# 2. Adjuntar a la VM (en caliente, sin apagarla)
virsh attach-disk debian-lab \
  /var/lib/libvirt/images/data-disk.qcow2 vdb \
  --driver qemu --subdriver qcow2 --persistent
```

Desglose de los argumentos:

| Argumento | Significado |
|-----------|-------------|
| `debian-lab` | Nombre de la VM |
| `/var/lib/.../data-disk.qcow2` | Path a la imagen de disco |
| `vdb` | Target device en la VM (cómo se llamará dentro) |
| `--driver qemu` | QEMU maneja el disco (siempre para imágenes) |
| `--subdriver qcow2` | Formato de la imagen |
| `--persistent` | Guardar en el XML de la VM (sobrevive reinicio) |

### Sin --persistent: disco temporal

```bash
virsh attach-disk debian-lab \
  /var/lib/libvirt/images/temp-disk.qcow2 vdc \
  --driver qemu --subdriver qcow2
# Sin --persistent: el disco se pierde al apagar la VM
```

Útil para pruebas rápidas donde no quieres modificar la definición de la VM.

### Adjuntar con bus específico

```bash
# Con virtio-scsi (aparece como /dev/sdb en la VM)
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
# Desde el host: ¿qué discos tiene la VM?
virsh domblklist debian-lab
#  Target   Source
# ─────────────────────────────────────────────────────
#  vda      /var/lib/libvirt/images/debian-lab.qcow2
#  vdb      /var/lib/libvirt/images/data-disk.qcow2

# Más detalle
virsh domblkinfo debian-lab vdb
#  Capacity:    5368709120        (5 GiB)
#  Allocation:  196608            (192 KiB — thin provisioning)
#  Physical:    196608

# Dentro de la VM
lsblk
# vda  252:0   0  20G  0 disk
# ├─vda1 ...
# vdb  252:16  0   5G  0 disk    ← nuevo disco, sin particiones
```

---

## 4. Quitar discos con virsh detach-disk

```bash
# Quitar disco por target
virsh detach-disk debian-lab vdb --persistent

# Si la VM está corriendo, el disco se desconecta en caliente
# Si --persistent, también se elimina del XML
```

**Importante**: `detach-disk` desconecta el disco de la VM pero **no elimina el archivo de imagen**. El archivo qcow2/raw sigue en el host:

```bash
ls -lh /var/lib/libvirt/images/data-disk.qcow2
# -rw------- 1 qemu qemu 196K ...    ← sigue ahí
```

Para eliminar completamente:

```bash
virsh detach-disk debian-lab vdb --persistent
rm /var/lib/libvirt/images/data-disk.qcow2
```

### Precauciones antes de detach

```bash
# 1. Dentro de la VM: desmontar el filesystem
sudo umount /data

# 2. Verificar que no haya procesos usándolo
sudo lsof +f -- /data

# 3. Desde el host: detach
virsh detach-disk debian-lab vdb --persistent
```

Si haces detach sin desmontar primero, la VM verá errores de I/O en los procesos que usaban ese disco.

---

## 5. Añadir discos editando el XML de la VM

Para configuraciones más complejas (cache mode, io mode, serial number), editar el XML directamente da control total:

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
| | `cache='none'` | Modo de caché (ver sección 8) |
| | `io='native'` | Usa I/O nativo del kernel (vs `threads`) |
| | `discard='unmap'` | Soportar TRIM desde la VM |
| `<source>` | `file='...'` | Ruta al archivo de imagen |
| `<target>` | `dev='vdb'` | Nombre del dispositivo en la VM |
| | `bus='virtio'` | Bus a usar |
| `<address>` | (auto) | Dirección PCI — libvirt la asigna automáticamente |

### Añadir un disco via XML

```bash
virsh edit debian-lab
```

Buscar la sección `<devices>` y añadir antes de `</devices>`:

```xml
    <disk type='file' device='disk'>
      <driver name='qemu' type='qcow2' cache='none' discard='unmap'/>
      <source file='/var/lib/libvirt/images/extra-disk.qcow2'/>
      <target dev='vdb' bus='virtio'/>
    </disk>
```

No incluyas `<address>` — libvirt la genera automáticamente al guardar.

Después de guardar, reiniciar la VM para que tome el cambio (los cambios con `virsh edit` no aplican en caliente).

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

Redimensionar un disco virtual tiene **tres pasos**: expandir la imagen, expandir la partición, expandir el filesystem. Omitir cualquiera deja espacio inutilizable.

### Paso 1: expandir la imagen (host)

```bash
# Apagar la VM (recomendado, aunque qemu-img resize funciona con VM corriendo
# si usas virsh blockresize en su lugar)
virsh shutdown debian-lab

# Esperar a que apague
virsh list --all | grep debian-lab

# Expandir la imagen 5 GiB
qemu-img resize /var/lib/libvirt/images/debian-lab.qcow2 +5G
# Image resized.

# Verificar
qemu-img info /var/lib/libvirt/images/debian-lab.qcow2 | grep "virtual size"
# virtual size: 25 GiB
```

Alternativa en caliente (VM corriendo):

```bash
virsh blockresize debian-lab /var/lib/libvirt/images/debian-lab.qcow2 25G
# Block device '/var/lib/libvirt/images/debian-lab.qcow2' is resized
```

### Paso 2: expandir la partición (dentro de la VM)

Arrancar la VM y verificar que el disco creció:

```bash
lsblk
# vda    252:0    0   25G  0 disk       ← 25G (antes era 20G)
# ├─vda1 252:1    0    1M  0 part
# ├─vda2 252:2    0    1G  0 part /boot
# └─vda3 252:3    0   19G  0 part /      ← sigue en 19G
```

El disco tiene 25G pero `vda3` sigue en 19G. Hay 5G libres al final del disco.

Expandir la partición con `growpart` (de cloud-utils-growpart):

```bash
# Instalar si no está disponible
sudo apt install cloud-guest-utils    # Debian/Ubuntu
sudo dnf install cloud-utils-growpart # Fedora

# Expandir la partición 3 para usar todo el espacio disponible
sudo growpart /dev/vda 3
# CHANGED: partition=3 start=2101248 old: size=39841759 end=41943007
#                                    new: size=50329567 end=52430815

lsblk
# └─vda3 252:3    0   24G  0 part /     ← ahora 24G
```

`growpart` modifica la tabla de particiones para que la partición ocupe el espacio libre contiguo. Es seguro en particiones montadas.

Alternativa sin growpart — con `fdisk` (más manual):

```bash
# ⚠️ Método manual — growpart es más seguro
sudo fdisk /dev/vda
# d → 3          (borrar partición 3 — solo la entrada, no los datos)
# n → 3 → ↵ → ↵  (recrear con mismo inicio, usar todo el espacio)
# N               (no eliminar la firma del filesystem)
# w               (escribir)
sudo partprobe /dev/vda
```

### Paso 3: expandir el filesystem (dentro de la VM)

La partición ahora tiene 24G pero el filesystem sigue viendo 19G:

```bash
df -h /
# Filesystem  Size  Used  Avail  Use%  Mounted on
# /dev/vda3    19G   5G    14G   27%   /
```

Expandir según el tipo de filesystem:

```bash
# ext4: resize2fs (funciona montado)
sudo resize2fs /dev/vda3
# resize2fs: Filesystem at /dev/vda3 is mounted on /; on-line resizing required
# Resizing the filesystem on /dev/vda3 to 6291195 (4k) blocks.
# The filesystem on /dev/vda3 is now 6291195 (4k) blocks long.

# XFS: xfs_growfs (usa el mountpoint, no el device)
sudo xfs_growfs /

# Btrfs:
sudo btrfs filesystem resize max /
```

Verificar:

```bash
df -h /
# Filesystem  Size  Used  Avail  Use%  Mounted on
# /dev/vda3    24G   5G    19G   21%   /     ← ahora 24G
```

### Diagrama del flujo completo

```
 HOST                                    VM
┌───────────────────────────┐    ┌──────────────────────────┐
│                           │    │                          │
│ 1. virsh shutdown vm      │    │                          │
│         │                 │    │                          │
│ 2. qemu-img resize +5G   │    │                          │
│         │                 │    │                          │
│ 3. virsh start vm ────────┼───►│ 4. lsblk (ver 25G)      │
│                           │    │         │                │
│                           │    │ 5. growpart /dev/vda 3   │
│                           │    │    (expandir partición)  │
│                           │    │         │                │
│                           │    │ 6. resize2fs /dev/vda3   │
│                           │    │    (expandir filesystem) │
│                           │    │         │                │
│                           │    │ 7. df -h (verificar)     │
│                           │    │                          │
└───────────────────────────┘    └──────────────────────────┘
```

### Redimensionar un disco de datos (no el del SO)

Para discos de datos es más sencillo porque puedes desmontar:

```bash
# Host: expandir la imagen
qemu-img resize /var/lib/libvirt/images/data-disk.qcow2 +5G

# VM: expandir partición y filesystem
sudo growpart /dev/vdb 1
sudo resize2fs /dev/vdb1    # ext4
# o
sudo xfs_growfs /data       # XFS

df -h /data
```

---

## 7. Backing files en producción: linked clones con virsh

En T01 viste backing files con `qemu-img`. Aquí los usamos para crear VMs reales rápidamente a partir de una plantilla:

### Preparar la imagen base

```bash
# 1. Instalar y configurar una VM "golden image"
# (instalar paquetes, configurar SSH, etc.)

# 2. Apagar la VM
virsh shutdown debian-base

# 3. Marcar la imagen como base (opcional pero recomendado)
# Simplemente dejar de usar esta VM directamente
```

### Crear VMs con linked clones

```bash
# 1. Crear overlay para cada VM
qemu-img create -f qcow2 \
  -b /var/lib/libvirt/images/debian-base.qcow2 -F qcow2 \
  /var/lib/libvirt/images/lab-vm1.qcow2

qemu-img create -f qcow2 \
  -b /var/lib/libvirt/images/debian-base.qcow2 -F qcow2 \
  /var/lib/libvirt/images/lab-vm2.qcow2

# 2. Clonar la definición de la VM (no el disco)
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
# disk size: 196 KiB              ← casi nada
#
# image: debian-base.qcow2
# disk size: 3.1 GiB              ← la base compartida
```

### Alternativa rápida: virt-clone con --reflink

Si el filesystem del host soporta reflink (Btrfs, XFS con reflink):

```bash
virt-clone --original debian-base --name lab-vm1 --auto-clone
# En Btrfs/XFS con reflink, la copia es instantánea y no duplica datos
```

---

## 8. Cache modes: rendimiento vs seguridad

El cache mode controla cómo QEMU maneja el caché de I/O entre la VM y el disco del host. Es una de las configuraciones más impactantes en rendimiento.

### Los modos principales

```
 VM escribe datos → ¿dónde se almacenan temporalmente?

 none:       VM → QEMU → directamente al disco host (sin caché de QEMU)
             El filesystem del host puede cachear en su page cache.
             Seguro ante cortes de luz.

 writethrough: VM → QEMU → caché QEMU (lectura) + disco host (escritura)
               Las lecturas se cachean, las escrituras van directo al disco.
               Seguro pero más lento que writeback.

 writeback:  VM → QEMU → caché QEMU → disco host (cuando convenga)
             QEMU cachea lecturas Y escrituras en RAM.
             Más rápido pero datos pueden perderse en corte de luz.

 unsafe:     VM → QEMU → caché sin flush
             Ignora flush del guest. Máximo rendimiento.
             SOLO para pruebas — pérdida de datos garantizada ante fallo.
```

### Comparación

| Modo | Rendimiento | Seguridad de datos | Uso recomendado |
|------|:-----------:|:------------------:|-----------------|
| `none` | Bueno | Alta | **Default recomendado** para producción |
| `writethrough` | Medio | Alta | VMs que necesitan garantía total |
| `writeback` | Alto | Media | Labs, desarrollo |
| `unsafe` | Máximo | Ninguna | Instalación de SOs, pruebas desechables |
| `directsync` | Bajo | Máxima | Bases de datos críticas |

### Configurar cache mode

```bash
# Con virsh attach-disk
virsh attach-disk debian-lab \
  /var/lib/libvirt/images/data-disk.qcow2 vdb \
  --driver qemu --subdriver qcow2 --cache none --persistent

# En el XML (virsh edit)
<driver name='qemu' type='qcow2' cache='none'/>
```

### io mode: threads vs native

Complementario al cache mode:

| io mode | Mecanismo | Cuándo usar |
|---------|-----------|-------------|
| `threads` (default) | Pool de threads para I/O | cache=writeback, cache=writethrough |
| `native` | Linux AIO (io_uring / libaio) | cache=none (recomendado) |

```xml
<driver name='qemu' type='qcow2' cache='none' io='native'/>
```

### discard='unmap': soporte TRIM

Permite que la VM envíe comandos TRIM/discard al host, lo que libera clusters qcow2 cuando la VM borra archivos:

```xml
<driver name='qemu' type='qcow2' cache='none' discard='unmap'/>
```

Dentro de la VM:

```bash
# Liberar espacio no usado
sudo fstrim -v /
# /: 2.1 GiB trimmed

# O montar con opción discard para TRIM automático
# (en fstab dentro de la VM)
UUID=...  /  ext4  defaults,discard  0  1
```

Con `discard='unmap'`, el archivo qcow2 en el host puede **reducir su tamaño** cuando la VM borra archivos — algo que sin TRIM nunca ocurre.

---

## 9. Flujo completo de laboratorio

Este flujo resume todo C11 — desde crear una imagen hasta tener una VM con múltiples discos completamente funcional:

### Escenario: VM con disco de sistema + disco de datos

```bash
# ═══════════════════════════════════════════════
# EN EL HOST
# ═══════════════════════════════════════════════

# 1. Crear imagen para disco de datos
qemu-img create -f qcow2 /var/lib/libvirt/images/debian-data.qcow2 10G

# 2. Adjuntar a la VM
virsh attach-disk debian-lab \
  /var/lib/libvirt/images/debian-data.qcow2 vdb \
  --driver qemu --subdriver qcow2 --persistent

# 3. Verificar
virsh domblklist debian-lab
# Target   Source
# vda      /var/lib/libvirt/images/debian-lab.qcow2
# vdb      /var/lib/libvirt/images/debian-data.qcow2


# ═══════════════════════════════════════════════
# DENTRO DE LA VM
# ═══════════════════════════════════════════════

# 4. Verificar nuevo disco
lsblk
# vdb  252:16  0  10G  0 disk

# 5. Crear tabla de particiones GPT
sudo fdisk /dev/vdb
# g → n → ↵ → ↵ → ↵ → w

# 6. Crear filesystem
sudo mkfs.ext4 -L "DATA" /dev/vdb1

# 7. Crear mount point y montar
sudo mkdir -p /data
sudo mount /dev/vdb1 /data

# 8. Configurar montaje persistente
sudo blkid /dev/vdb1
# UUID="xxxx-yyyy-zzzz" TYPE="ext4"

echo 'UUID=xxxx-yyyy-zzzz  /data  ext4  defaults,noatime,nofail  0  2' \
  | sudo tee -a /etc/fstab

# 9. Verificar fstab
sudo umount /data
sudo mount -a
df -h /data
# /dev/vdb1  9.8G  24K  9.3G  1%  /data  ✓

# 10. Crear datos de prueba
echo "Lab operativo" | sudo tee /data/status.txt


# ═══════════════════════════════════════════════
# DE VUELTA EN EL HOST — verificar
# ═══════════════════════════════════════════════

# 11. ¿Cuánto ocupa realmente?
qemu-img info /var/lib/libvirt/images/debian-data.qcow2
# virtual size: 10 GiB
# disk size: 33.1 MiB     ← thin provisioning en acción

# 12. Snapshot antes de experimentar
virsh snapshot-create-as debian-lab snap-lab-ready \
  --description "Lab configurado con disco de datos"
```

### Escenario: expandir el disco de datos después

```bash
# HOST: expandir imagen
virsh shutdown debian-lab
qemu-img resize /var/lib/libvirt/images/debian-data.qcow2 +5G
virsh start debian-lab

# VM: expandir partición y filesystem
sudo growpart /dev/vdb 1
sudo resize2fs /dev/vdb1
df -h /data
# /dev/vdb1  14.7G  24K  13.9G  1%  /data  ← 10G → 15G
```

---

## 10. Errores comunes

### Error 1: attach-disk sin --subdriver

```bash
virsh attach-disk debian-lab \
  /var/lib/libvirt/images/disk.qcow2 vdb \
  --driver qemu --persistent
# La VM puede no arrancar o ver datos corruptos
```

**Por qué falla**: sin `--subdriver qcow2`, QEMU puede intentar abrir la imagen como raw, interpretando los metadatos qcow2 como datos del disco.

**Solución**: siempre especificar el formato:

```bash
virsh attach-disk debian-lab \
  /var/lib/libvirt/images/disk.qcow2 vdb \
  --driver qemu --subdriver qcow2 --persistent
```

### Error 2: expandir la imagen pero olvidar growpart y resize2fs

```bash
qemu-img resize disk.qcow2 +10G
# "¡Ya está! El disco ahora tiene 10G más"
# Dentro de la VM: df -h / → sigue mostrando el tamaño original
```

**Por qué pasa**: `qemu-img resize` solo cambia el disco virtual. La tabla de particiones y el filesystem no se enteran.

**Solución**: el flujo tiene 3 pasos obligatorios:

```bash
qemu-img resize disk.qcow2 +10G     # 1. imagen
sudo growpart /dev/vda 3              # 2. partición
sudo resize2fs /dev/vda3              # 3. filesystem
```

### Error 3: detach sin desmontar primero

```bash
# Dentro de la VM: /data está montado en /dev/vdb1
# En el host:
virsh detach-disk debian-lab vdb --persistent
# La VM empieza a mostrar errores de I/O
```

**Por qué pasa**: el kernel de la VM intenta leer/escribir un dispositivo que ya no existe.

**Solución**: siempre desmontar dentro de la VM primero:

```bash
# VM:
sudo umount /data
# Host:
virsh detach-disk debian-lab vdb --persistent
```

### Error 4: usar cache=writeback y perder datos

```bash
# Configurar caché agresivo para "mejor rendimiento"
<driver name='qemu' type='qcow2' cache='writeback'/>

# Corte de luz o kill -9 del proceso QEMU
# → datos en caché perdidos, filesystem potencialmente corrupto
```

**Por qué pasa**: `writeback` mantiene escrituras en RAM del host antes de enviarlas al disco. Un fallo pierde esas escrituras.

**Solución**: usar `cache='none'` para producción y labs con datos importantes. Reservar `writeback` para VMs desechables donde el rendimiento importa más que la integridad.

### Error 5: permisos incorrectos en la imagen

```bash
qemu-img create -f qcow2 /var/lib/libvirt/images/new-disk.qcow2 5G
virsh attach-disk debian-lab \
  /var/lib/libvirt/images/new-disk.qcow2 vdb \
  --driver qemu --subdriver qcow2 --persistent
# error: Cannot access storage file: Permission denied
```

**Por qué pasa**: QEMU corre como usuario `qemu` (o `libvirt-qemu`). El archivo recién creado pertenece a tu usuario.

**Solución**:

```bash
sudo chown qemu:qemu /var/lib/libvirt/images/new-disk.qcow2
# o en Debian/Ubuntu:
sudo chown libvirt-qemu:kvm /var/lib/libvirt/images/new-disk.qcow2

# Alternativa: crear como root directamente en el directorio de imágenes
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
│   growpart /dev/vda 3             2. Expandir partición (VM)    │
│   resize2fs /dev/vda3       (ext4) 3. Expandir filesystem (VM)  │
│   xfs_growfs /              (xfs)                                │
│   btrfs filesystem resize   (btrfs)                              │
│     max /mountpoint                                              │
├──────────────────────────────────────────────────────────────────┤
│ LINKED CLONES                                                    │
│   qemu-img create -f qcow2       Crear overlay                  │
│     -b base.qcow2 -F qcow2                                      │
│     overlay.qcow2                                                │
│   virt-clone --original base-vm   Clonar definición de VM       │
│     --name clone --file overlay                                  │
│     --preserve-data                                              │
├──────────────────────────────────────────────────────────────────┤
│ CACHE MODES                                                      │
│   none          Seguro, buen rendimiento (recomendado)          │
│   writethrough  Seguro, más lento                                │
│   writeback     Rápido, riesgo ante fallos                      │
│   unsafe        Máximo rendimiento, solo para pruebas           │
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

### Ejercicio 1: Ciclo completo de disco de datos

Realiza el flujo completo de la sección 9:

1. En el host, crea una imagen qcow2 de 3 GiB.
2. Adjúntala a tu VM como `vdb` con `virsh attach-disk` (con `--persistent`).
3. Dentro de la VM:
   - Verifica con `lsblk` que `vdb` aparece.
   - Crea tabla GPT + una partición con `fdisk`.
   - Formatea con ext4 (label "EXERCISE").
   - Monta en `/exercise`.
   - Crea archivos de prueba.
   - Configura en fstab con UUID y `nofail`.
   - Verifica con `mount -a`.
4. Desde el host, ejecuta `qemu-img info` sobre la imagen. ¿Cuánto ocupa realmente vs su tamaño virtual?
5. Reinicia la VM y verifica que `/exercise` sigue montado.
6. Desmonta, haz detach y borra la imagen.

> **Pregunta de reflexión**: ¿por qué usamos `nofail` en fstab para este disco de datos? ¿Qué pasaría si haces `virsh detach-disk` sin quitar primero la línea de fstab y reinicias la VM?

### Ejercicio 2: Redimensionar disco en vivo

1. Crea y adjunta un disco qcow2 de 2 GiB a tu VM.
2. Dentro de la VM, particiona (GPT, una partición), formatea (ext4) y monta en `/grow-test`.
3. Crea un archivo de 500 MiB para ocupar espacio:
   ```bash
   sudo dd if=/dev/urandom of=/grow-test/bigfile bs=1M count=500
   ```
4. Verifica con `df -h /grow-test` cuánto espacio queda.
5. Desde el host, expande la imagen a 5 GiB:
   ```bash
   qemu-img resize <imagen> 5G
   ```
6. Dentro de la VM, verifica con `lsblk` que el disco ahora muestra 5G pero la partición sigue en 2G.
7. Expande la partición con `growpart` y el filesystem con `resize2fs`.
8. Verifica con `df -h` que ahora tienes ~5G disponibles y el archivo de 500 MiB sigue intacto.

> **Pregunta de reflexión**: ¿por qué `growpart` puede expandir una partición mientras está montada sin perder datos? ¿Qué sería diferente si tuvieras que usar `fdisk` manualmente para lo mismo?

### Ejercicio 3: Linked clones para lab multi-VM

Si tienes una VM base instalada y configurada:

1. Apaga la VM base.
2. Crea dos overlays:
   ```bash
   qemu-img create -f qcow2 -b /var/lib/libvirt/images/<base>.qcow2 \
     -F qcow2 /var/lib/libvirt/images/clone1.qcow2
   qemu-img create -f qcow2 -b /var/lib/libvirt/images/<base>.qcow2 \
     -F qcow2 /var/lib/libvirt/images/clone2.qcow2
   ```
3. Clona la definición de la VM:
   ```bash
   virt-clone --original <base-vm> --name clone1 \
     --file /var/lib/libvirt/images/clone1.qcow2 \
     --preserve-data --check path_in_use=off
   ```
4. Arranca `clone1`. ¿Funciona? ¿Tiene el mismo hostname y configuración que la base?
5. Haz un cambio visible en `clone1` (cambiar hostname, crear archivo).
6. Compara los tamaños con `qemu-img info`:
   - ¿Cuánto ocupa la base?
   - ¿Cuánto ocupa clone1 después de los cambios?
   - ¿Cuánto ocupa clone2 (sin arrancar)?
7. Verifica la cadena con `qemu-img info --backing-chain clone1.qcow2`.
8. Limpia: apaga y elimina los clones (`virsh destroy`, `virsh undefine`, `rm`).

> **Pregunta de reflexión**: si necesitaras 10 VMs de laboratorio idénticas, ¿cuánto espacio ahorrarías con linked clones vs copias completas? ¿Cuál es el punto débil de este esquema? (Pista: ¿qué pasa si el archivo base se corrompe?)

---

**C11 completado.** Con los 6 tópicos de este capítulo tienes las bases para gestionar discos en Linux y en QEMU/KVM: desde identificar dispositivos de bloque y crear particiones, hasta formatear filesystems, configurar montaje persistente, y trabajar con imágenes de disco virtuales. En B08 profundizarás en LVM, RAID y almacenamiento avanzado usando estas herramientas como base.
