# Dispositivos de Bloque

## Índice

1. [De /dev a discos: construyendo sobre C01](#1-de-dev-a-discos-construyendo-sobre-c01)
2. [Nomenclatura completa de discos en Linux](#2-nomenclatura-completa-de-discos-en-linux)
3. [lsblk: el mapa de tus discos](#3-lsblk-el-mapa-de-tus-discos)
4. [blkid: identificar qué hay en cada dispositivo](#4-blkid-identificar-qué-hay-en-cada-dispositivo)
5. [/dev/disk/by-*: nombres estables](#5-devdiskby--nombres-estables)
6. [Major y minor numbers en contexto](#6-major-y-minor-numbers-en-contexto)
7. [Loop devices: archivos que actúan como discos](#7-loop-devices-archivos-que-actúan-como-discos)
8. [Dispositivos de bloque en QEMU/KVM](#8-dispositivos-de-bloque-en-qemukvm)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. De /dev a discos: construyendo sobre C01

En C01 (T03_dev) aprendiste que `/dev` contiene archivos que representan dispositivos, y que los **dispositivos de bloque** (marcados con `b` en `ls -la`) almacenan datos en bloques de tamaño fijo con acceso aleatorio. Este tópico profundiza en los dispositivos de bloque que representan **discos y sus particiones** — la base que necesitas para gestionar almacenamiento en VMs.

Un dispositivo de bloque es la interfaz del kernel hacia un medio de almacenamiento. Cuando ves `/dev/sda`, no estás viendo "el disco" — estás viendo un **archivo especial** que el kernel traduce en operaciones de lectura/escritura hacia el hardware real (o virtual):

```
 Espacio de usuario          Kernel              Hardware/Virtual
┌──────────────┐      ┌──────────────────┐     ┌──────────────┐
│ cat, dd, mkfs│      │                  │     │              │
│ fdisk, mount │─────►│ Block layer      │────►│ Disco SATA   │
│              │      │ (I/O scheduler,  │     │ SSD NVMe     │
│ Lee/escribe  │      │  cache, buffers) │     │ Disco virtual │
│ /dev/sda     │      │                  │     │ (qcow2/raw)  │
└──────────────┘      └──────────────────┘     └──────────────┘
```

Todo programa que trabaja con discos (desde `dd` hasta `fdisk`) simplemente abre y lee/escribe un archivo en `/dev`. El kernel se encarga de traducir esas operaciones al hardware real — o al archivo de imagen de disco en el caso de QEMU.

---

## 2. Nomenclatura completa de discos en Linux

Cada tipo de controlador de disco usa un prefijo distinto en `/dev`. Conocer la nomenclatura te dice inmediatamente qué tipo de hardware (o virtualización) hay detrás.

### Discos SCSI/SATA/USB: /dev/sd*

```
/dev/sda        ← primer disco SCSI/SATA/USB
/dev/sda1       ← primera partición del primer disco
/dev/sda2       ← segunda partición
/dev/sdb        ← segundo disco
/dev/sdb1       ← primera partición del segundo disco
```

La letra (`a`, `b`, `c`...) indica el orden en que el kernel detectó los discos. El número indica la partición. **Importante**: el orden puede cambiar entre reinicios si el hardware cambia — por eso se usan UUIDs en fstab, no nombres `/dev/sd*`.

### Discos NVMe: /dev/nvme*

Los SSDs NVMe usan un esquema diferente porque admiten múltiples namespaces (conjuntos lógicos de bloques):

```
/dev/nvme0          ← controlador NVMe #0 (el chip)
/dev/nvme0n1        ← namespace 1 del controlador 0 (el "disco")
/dev/nvme0n1p1      ← partición 1
/dev/nvme0n1p2      ← partición 2
```

Nomenclatura: `nvme<controlador>n<namespace>p<partición>`. En la práctica, la mayoría de SSDs tienen un solo namespace, así que casi siempre ves `nvme0n1`.

### Discos Virtio: /dev/vd*

Los discos que QEMU/KVM presenta a las VMs usando **paravirtualización virtio**:

```
/dev/vda        ← primer disco virtio
/dev/vda1       ← primera partición
/dev/vda2       ← segunda partición
/dev/vdb        ← segundo disco virtio
```

El esquema es idéntico a `sd*` pero con prefijo `vd`. Si ves `vd*` sabes que estás dentro de una VM con virtio. Si la VM usa emulación IDE/SATA en lugar de virtio, los discos aparecen como `/dev/sda` — indistinguibles de hardware real.

### Discos IDE emulados: /dev/hd*

Nomenclatura legacy para discos IDE:

```
/dev/hda        ← master del primer canal IDE
/dev/hdb        ← slave del primer canal IDE
```

Raro en sistemas modernos, pero QEMU puede emular IDE para VMs con kernels antiguos.

### Resumen de nomenclatura

```
 Tipo de disco          Dispositivo        Partición 1      Dónde lo ves
─────────────────────────────────────────────────────────────────────────
 SATA/SCSI/USB          /dev/sda           /dev/sda1        Host físico
 NVMe                   /dev/nvme0n1       /dev/nvme0n1p1   Host físico
 Virtio (QEMU)          /dev/vda           /dev/vda1        Dentro de VM
 IDE emulado            /dev/hda           /dev/hda1        VMs legacy
 Loop device            /dev/loop0         /dev/loop0p1     Ambos
```

---

## 3. lsblk: el mapa de tus discos

`lsblk` (list block devices) muestra todos los dispositivos de bloque en forma de árbol, revelando la relación disco → partición → filesystem:

### Uso básico

```bash
lsblk
```

```
NAME        MAJ:MIN RM   SIZE RO TYPE MOUNTPOINTS
sda           8:0    0 476.9G  0 disk
├─sda1        8:1    0   600M  0 part /boot/efi
├─sda2        8:2    0     1G  0 part /boot
└─sda3        8:3    0 475.4G  0 part
  ├─fedora-root 253:0  0    70G  0 lvm  /
  ├─fedora-swap 253:1  0   7.8G  0 lvm  [SWAP]
  └─fedora-home 253:2  0 397.5G  0 lvm  /home
```

Cada columna:

| Columna | Significado |
|---------|-------------|
| **NAME** | Nombre del dispositivo (sin `/dev/`) |
| **MAJ:MIN** | Major y minor numbers del kernel |
| **RM** | Removable: 1 = extraíble (USB), 0 = fijo |
| **SIZE** | Tamaño del dispositivo o partición |
| **RO** | Read-only: 1 = solo lectura |
| **TYPE** | `disk`, `part`, `lvm`, `loop`, `rom` |
| **MOUNTPOINTS** | Dónde está montado (vacío = no montado) |

El árbol visual es clave: la indentación muestra que `sda1`, `sda2`, `sda3` son particiones de `sda`, y que `fedora-root`, `fedora-swap`, `fedora-home` son volúmenes LVM dentro de `sda3`.

### Dentro de una VM con virtio

```bash
lsblk
```

```
NAME   MAJ:MIN RM  SIZE RO TYPE MOUNTPOINTS
vda    252:0    0   20G  0 disk
├─vda1 252:1    0    1M  0 part
├─vda2 252:2    0    1G  0 part /boot
└─vda3 252:3    0   19G  0 part /
vdb    252:16   0    5G  0 disk
```

Observa: `vda` (disco del sistema) con tres particiones, y `vdb` (disco adicional) sin particiones — un disco en blanco recién añadido.

### Opciones útiles de lsblk

```bash
# Mostrar UUIDs y tipos de filesystem
lsblk -f

# NAME   FSTYPE FSVER LABEL UUID                                 MOUNTPOINTS
# vda
# ├─vda1
# ├─vda2 ext4   1.0         a1b2c3d4-...                         /boot
# └─vda3 xfs                e5f6a7b8-...                         /
# vdb

# Mostrar columnas específicas
lsblk -o NAME,SIZE,TYPE,FSTYPE,MOUNTPOINT,UUID

# Mostrar todos los dispositivos (incluidos vacíos)
lsblk -a

# Mostrar en formato lista (sin árbol)
lsblk -l

# Mostrar solo discos (sin particiones)
lsblk -d

# Salida en JSON (útil para scripts)
lsblk -J

# Mostrar rutas completas
lsblk -p
# /dev/vda
# ├─/dev/vda1
# ├─/dev/vda2  ext4  ...  /boot
# └─/dev/vda3  xfs   ...  /
```

### lsblk -f: la opción que más usarás

`-f` combina nombre, tipo de filesystem, label, UUID y punto de montaje — todo lo necesario para decidir qué hacer con un disco:

```bash
lsblk -f
```

```
NAME   FSTYPE FSVER LABEL   UUID                                 MOUNTPOINTS
vda
├─vda1
├─vda2 ext4   1.0           a1b2c3d4-e5f6-7890-abcd-111111111111 /boot
└─vda3 xfs                  e5f6a7b8-c9d0-1234-efab-222222222222 /
vdb
```

`vdb` no tiene FSTYPE ni UUID — está completamente vacío: sin tabla de particiones, sin filesystem. En T02 aprenderás a particionarlo, y en S02 a darle formato.

---

## 4. blkid: identificar qué hay en cada dispositivo

`blkid` (block device identifier) examina las **firmas** (magic bytes) de cada dispositivo para determinar qué filesystem contiene:

```bash
sudo blkid
```

```
/dev/vda2: UUID="a1b2c3d4-e5f6-7890-abcd-111111111111" BLOCK_SIZE="4096" TYPE="ext4" PARTUUID="abcd1234-02"
/dev/vda3: UUID="e5f6a7b8-c9d0-1234-efab-222222222222" BLOCK_SIZE="512" TYPE="xfs" PARTUUID="abcd1234-03"
/dev/vda1: PARTUUID="abcd1234-01"
```

Diferencias con `lsblk -f`:

| Aspecto | lsblk -f | blkid |
|---------|----------|-------|
| Necesita root | No | Sí (para ver todo) |
| Muestra árbol | Sí | No |
| Muestra PARTUUID | No | Sí |
| Muestra BLOCK_SIZE | No | Sí |
| Detecta sin montar | Indirecto (lee caché) | Directo (lee el disco) |

### Consultar un dispositivo específico

```bash
sudo blkid /dev/vda2
# /dev/vda2: UUID="a1b2c3d4-..." TYPE="ext4" PARTUUID="abcd1234-02"
```

### Buscar por UUID

```bash
sudo blkid -U "a1b2c3d4-e5f6-7890-abcd-111111111111"
# /dev/vda2
```

Útil en scripts que necesitan encontrar un dispositivo por su UUID.

### Buscar por tipo de filesystem

```bash
sudo blkid -t TYPE=xfs
# /dev/vda3: UUID="e5f6a7b8-..." TYPE="xfs" ...
```

### UUID vs PARTUUID

Son identificadores distintos:

```
 UUID          ← Generado por mkfs al crear el filesystem
                 Cambia si reformateas la partición
                 Usado en /etc/fstab para montar

 PARTUUID      ← Generado por fdisk/gdisk al crear la partición
                 Cambia si recreas la partición
                 Independiente del filesystem
                 Usado principalmente con GPT
```

En la práctica, `/etc/fstab` usa UUID (del filesystem) porque es lo que el kernel necesita para montar.

---

## 5. /dev/disk/by-*: nombres estables

Los nombres como `/dev/sda` pueden cambiar entre reinicios (el kernel asigna letras según el orden de detección). Los directorios bajo `/dev/disk/` contienen **symlinks estables** que apuntan al dispositivo real:

```bash
ls -la /dev/disk/by-uuid/
# a1b2c3d4-e5f6-7890-abcd-111111111111 -> ../../vda2
# e5f6a7b8-c9d0-1234-efab-222222222222 -> ../../vda3

ls -la /dev/disk/by-id/
# virtio-serial12345-part1 -> ../../vda1
# virtio-serial12345-part2 -> ../../vda2
# ata-Samsung_SSD_860_S3Z1234567-part1 -> ../../sda1   # en host físico

ls -la /dev/disk/by-path/
# pci-0000:04:00.0-part1 -> ../../vda1
# pci-0000:04:00.0-part2 -> ../../vda2
```

### Los 4 directorios by-*

| Directorio | Clave | Estabilidad | Uso típico |
|------------|-------|:-----------:|------------|
| `by-uuid` | UUID del filesystem | Alta (cambia si reformateas) | `/etc/fstab` |
| `by-id` | Serial del disco + número | Alta (única por hardware) | Scripts, udev rules |
| `by-path` | Posición en el bus PCI | Media (cambia si mueves el disco de slot) | Identidad topológica |
| `by-partuuid` | UUID de la partición GPT | Alta (cambia si recreas la partición) | Bootloader, ESP |
| `by-label` | Label del filesystem | Alta (tú la controlas) | Conveniencia (`DATOS`, `BACKUP`) |

### Por qué importa en VMs

Dentro de una VM con un solo disco virtio, `/dev/vda` es predecible. Pero cuando añades múltiples discos:

```bash
# VM con 3 discos virtio
lsblk -d
# vda    20G   ← ¿cuál es cuál?
# vdb     5G
# vdc    10G
```

Los nombres `vdb`/`vdc` pueden intercambiarse entre reinicios. Usar `by-uuid` o `by-id` garantiza que montas el disco correcto:

```bash
# Seguro: UUID no cambia aunque cambie la letra
UUID=e5f6a7b8-... /data xfs defaults 0 0

# Inseguro: la letra puede cambiar
/dev/vdb1 /data xfs defaults 0 0
```

---

## 6. Major y minor numbers en contexto

En C01 T03_dev viste que cada device node tiene un par major:minor. Aquí los ponemos en contexto con discos:

```bash
ls -la /dev/vda*
# brw-rw---- 1 root disk 252,  0 ... /dev/vda
# brw-rw---- 1 root disk 252,  1 ... /dev/vda1
# brw-rw---- 1 root disk 252,  2 ... /dev/vda2
# brw-rw---- 1 root disk 252,  3 ... /dev/vda3
```

- **Major number (252)**: identifica el **driver** — en este caso, el driver virtio-blk.
- **Minor number (0, 1, 2, 3)**: identifica el dispositivo específico dentro de ese driver — 0 es el disco entero, 1-3 son las particiones.

Major numbers comunes para discos:

| Major | Driver | Dispositivos |
|:-----:|--------|-------------|
| 8 | sd (SCSI/SATA/USB) | `/dev/sda` (8,0), `/dev/sda1` (8,1), `/dev/sdb` (8,16) |
| 259 | blkext (NVMe) | `/dev/nvme0n1` (259,0), `/dev/nvme0n1p1` (259,1) |
| 252 | virtio-blk | `/dev/vda` (252,0), `/dev/vda1` (252,1) |
| 253 | device-mapper (LVM) | `/dev/dm-0` (253,0), `/dev/dm-1` (253,1) |
| 7 | loop | `/dev/loop0` (7,0), `/dev/loop1` (7,1) |

Para discos `sd*`, el minor calcula así: disco × 16 + partición. Así `sda` = (8,0), `sda1` = (8,1), `sdb` = (8,16), `sdb1` = (8,17). Esto limita a 15 particiones por disco — suficiente para la mayoría de casos.

```bash
# Ver major:minor con lsblk
lsblk -o NAME,MAJ:MIN

# Ver qué drivers están registrados para cada major
cat /proc/devices | grep -E "Block|252|253|8|7"
```

---

## 7. Loop devices: archivos que actúan como discos

Un **loop device** es un mecanismo del kernel que presenta un archivo regular como si fuera un dispositivo de bloque. Es la base de cómo QEMU trabaja con imágenes de disco, y es útil para practicar particiones sin disco real.

### Crear un loop device

```bash
# 1. Crear un archivo que simule un disco de 1 GiB
dd if=/dev/zero of=/tmp/disk-test.img bs=1M count=1024

# 2. Asociar el archivo a un loop device
sudo losetup /dev/loop0 /tmp/disk-test.img

# 3. Verificar
lsblk /dev/loop0
# loop0  7:0  0  1G  0 loop

losetup -l
# NAME       BACK-FILE              OFFSET  SIZELIMIT  ...
# /dev/loop0 /tmp/disk-test.img          0          0  ...
```

Ahora `/dev/loop0` se comporta exactamente como un disco real de 1 GiB. Puedes particionarlo con `fdisk`, formatearlo con `mkfs`, montarlo — todo sin tocar hardware real.

### Asociar automáticamente (primer loop libre)

```bash
sudo losetup -f /tmp/disk-test.img
# Usa el primer /dev/loop* disponible

sudo losetup -f
# Muestra cuál sería el siguiente loop libre (sin asociar nada)
```

### Detectar particiones en un loop device

Si el archivo contiene una tabla de particiones, necesitas decirle al kernel que las escanee:

```bash
# Crear particiones en el loop device (se verá en T02)
sudo fdisk /dev/loop0
# ... crear particiones ...

# Forzar relectura de la tabla de particiones
sudo partprobe /dev/loop0

# Ahora aparecen las particiones
lsblk /dev/loop0
# loop0   7:0  0   1G  0 loop
# ├─loop0p1  0  500M  0 part
# └─loop0p2  0  500M  0 part
```

### Desasociar

```bash
# Desasociar un loop device específico
sudo losetup -d /dev/loop0

# Desasociar todos
sudo losetup -D
```

### Por qué importan los loop devices

1. **Practicar sin riesgo**: puedes crear, particionar y formatear "discos" que son simples archivos.
2. **Montar imágenes ISO**: `mount -o loop imagen.iso /mnt` usa un loop device internamente.
3. **Entender QEMU**: QEMU hace algo conceptualmente similar — presenta un archivo (qcow2/raw) como disco a la VM. La diferencia es que QEMU traduce a nivel de VM, mientras que loop opera a nivel del kernel host.

---

## 8. Dispositivos de bloque en QEMU/KVM

Dentro de una VM con QEMU/KVM, los discos virtuales aparecen como dispositivos de bloque normales. La VM no sabe (ni le importa) que el "disco" es en realidad un archivo en el host.

### Cómo fluyen los datos

```
 VM (guest)                    Host
┌─────────────────┐     ┌────────────────────────┐
│ App escribe en  │     │                        │
│ /dev/vda1       │     │                        │
│       │         │     │                        │
│       ▼         │     │                        │
│ Kernel guest    │     │                        │
│ (driver virtio) │────►│ QEMU process           │
│                 │     │       │                │
└─────────────────┘     │       ▼                │
                        │ Escribe en archivo     │
                        │ /var/lib/libvirt/       │
                        │ images/debian-lab.qcow2 │
                        │       │                │
                        │       ▼                │
                        │ Kernel host            │
                        │ (escribe en /dev/sda)  │
                        └────────────────────────┘
```

### Comparar host vs VM

Un ejercicio revelador es comparar `lsblk` en ambos:

**En el host:**

```bash
lsblk
# sda            8:0    0  500G  0 disk
# ├─sda1         8:1    0  600M  0 part  /boot/efi
# ├─sda2         8:2    0    1G  0 part  /boot
# └─sda3         8:3    0  498G  0 part
#   └─fedora-root 253:0 0   70G  0 lvm   /
```

**Dentro de la VM:**

```bash
lsblk
# vda          252:0    0   20G  0 disk
# ├─vda1       252:1    0    1M  0 part
# ├─vda2       252:2    0    1G  0 part  /boot
# └─vda3       252:3    0   19G  0 part  /
```

Observa:
- El host tiene `sda` (SATA real) con major 8.
- La VM tiene `vda` (virtio) con major 252.
- La VM tiene su propia tabla de particiones, independiente del host.
- El "disco" de la VM (20G) vive como un archivo en el host.

### Verificar dónde están las imágenes de disco

```bash
# En el host, ver qué archivos usa una VM como discos
virsh domblklist debian-lab
#  Target   Source
# ──────────────────────────────────────────
#  vda      /var/lib/libvirt/images/debian-lab.qcow2
#  vdb      /var/lib/libvirt/images/extra-disk.qcow2

# Ver el tamaño real vs virtual
ls -lh /var/lib/libvirt/images/debian-lab.qcow2
# -rw------- 1 qemu qemu 4.2G ...   ← tamaño real en disco (thin provisioning)

qemu-img info /var/lib/libvirt/images/debian-lab.qcow2
# virtual size: 20 GiB               ← lo que la VM "ve"
# disk size: 4.2 GiB                 ← lo que ocupa realmente
# format: qcow2
```

Los 20 GiB que la VM ve como `/dev/vda` solo ocupan 4.2 GiB en el host — esto es **thin provisioning** de qcow2, que explorarás a fondo en S03.

---

## 9. Errores comunes

### Error 1: confundir el nombre del disco con su identidad

```bash
# "Mi disco de datos es /dev/vdb, lo pongo en fstab"
/dev/vdb1  /data  ext4  defaults  0  0
# Reinicio... /data tiene datos equivocados o no monta
```

**Por qué falla**: `/dev/vdb` es un nombre asignado por orden de detección. Si añades otro disco o cambias el orden en el XML de la VM, lo que era `vdb` puede pasar a ser `vdc`.

**Solución**: siempre usar UUID en fstab:

```bash
UUID=e5f6a7b8-c9d0-1234-efab-222222222222  /data  ext4  defaults  0  0
```

### Error 2: intentar operar en el disco entero en vez de la partición

```bash
sudo mkfs.ext4 /dev/vda
# ¡Esto DESTRUYE la tabla de particiones del disco entero!
# Querías decir /dev/vda1 o /dev/vdb1
```

**Por qué pasa**: `/dev/vda` es el disco, `/dev/vda1` es la partición. Formatear el disco entero destruye la tabla de particiones y todo su contenido.

**Solución**: siempre verificar con `lsblk` antes de ejecutar mkfs, fdisk, o dd:

```bash
lsblk
# Confirmar que operas en la partición correcta
```

### Error 3: lsblk no muestra un disco recién añadido a la VM

```bash
# Añadiste un disco con virsh attach-disk, pero no aparece
lsblk
# Solo vda... ¿dónde está vdb?
```

**Por qué pasa**: si usaste `attach-disk` sin `--persistent` y la VM no tenía el módulo de hotplug cargado, o si el bus no soporta hotplug.

**Solución**: verificar desde el host y dentro de la VM:

```bash
# En el host: ¿se adjuntó?
virsh domblklist debian-lab

# En la VM: forzar rescan de dispositivos SCSI (si aplica)
echo "- - -" | sudo tee /sys/class/scsi_host/host*/scan

# Para virtio, suele detectarse automáticamente. Si no, reiniciar la VM.
```

### Error 4: confundir UUID y PARTUUID

```bash
# En fstab pones PARTUUID cuando deberías poner UUID
PARTUUID=abcd1234-02  /boot  ext4  defaults  0  0
# Error: wrong fs type, bad option, bad superblock
```

**Por qué falla**: `PARTUUID` identifica la partición GPT, no el filesystem. El kernel necesita `UUID` (del filesystem) para montar.

**Solución**: usar `blkid` para ver ambos y elegir el correcto:

```bash
sudo blkid /dev/vda2
# UUID="a1b2c3d4-..." TYPE="ext4" PARTUUID="abcd1234-02"
#  ↑ usar este en fstab
```

### Error 5: no entender la diferencia entre lsblk y df

```bash
df -h
# Solo muestra filesystems montados

lsblk
# Muestra TODOS los dispositivos de bloque (montados o no)
```

**Confusión**: "df no muestra mi nuevo disco" — porque df muestra puntos de montaje, no dispositivos. Si un disco no está montado, df no lo ve.

**Cuándo usar cada uno**:
- `lsblk`: "¿qué discos/particiones existen?" — inventario completo.
- `df -h`: "¿cuánto espacio queda en los filesystems montados?" — capacidad.
- `blkid`: "¿qué filesystem tiene esta partición y cuál es su UUID?" — identificación.

---

## 10. Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│              DISPOSITIVOS DE BLOQUE — REFERENCIA                 │
├──────────────────────────────────────────────────────────────────┤
│ NOMENCLATURA                                                     │
│   /dev/sda, sdb...           SCSI/SATA/USB (host físico)        │
│   /dev/nvme0n1               NVMe SSD                           │
│   /dev/vda, vdb...           Virtio (dentro de VM)              │
│   /dev/loop0, loop1...       Loop devices (archivos como disco) │
│   /dev/dm-0, dm-1...         Device-mapper (LVM)                │
├──────────────────────────────────────────────────────────────────┤
│ INSPECCIÓN                                                       │
│   lsblk                      Árbol de discos y particiones      │
│   lsblk -f                   + filesystem, UUID, mountpoint     │
│   lsblk -d                   Solo discos (sin particiones)      │
│   lsblk -o NAME,SIZE,...     Columnas específicas               │
│   blkid                      UUIDs y tipos de filesystem        │
│   blkid /dev/vda1            Un dispositivo específico          │
│   blkid -U "<uuid>"          Buscar dispositivo por UUID        │
│   ls /dev/disk/by-uuid/      Symlinks estables por UUID         │
│   ls /dev/disk/by-id/        Symlinks estables por serial       │
├──────────────────────────────────────────────────────────────────┤
│ LOOP DEVICES                                                     │
│   dd if=/dev/zero of=x.img   Crear archivo-disco                │
│     bs=1M count=1024                                             │
│   losetup /dev/loop0 x.img   Asociar archivo a loop device      │
│   losetup -f x.img           Usar primer loop libre             │
│   losetup -l                 Listar loop devices activos        │
│   losetup -d /dev/loop0      Desasociar                         │
│   partprobe /dev/loop0       Re-leer tabla de particiones       │
├──────────────────────────────────────────────────────────────────┤
│ QEMU/KVM                                                         │
│   virsh domblklist <vm>      Discos de una VM                   │
│   qemu-img info <img>        Info de imagen de disco            │
│   lsblk (dentro de VM)       Ver discos virtuales               │
└──────────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: Inventario de dispositivos de bloque

En tu host (máquina física o VM donde trabajas):

1. Ejecuta `lsblk` y dibuja en papel el árbol de discos → particiones → LVM (si hay).
2. Ejecuta `lsblk -f` y anota qué filesystem tiene cada partición.
3. Ejecuta `sudo blkid` y compara los UUIDs con los que muestra `lsblk -f`. ¿Son los mismos? ¿Qué columnas extra muestra `blkid`?
4. Explora `/dev/disk/by-uuid/` y verifica que cada symlink apunta al dispositivo correcto.
5. Ejecuta `cat /proc/devices` y busca la sección "Block devices". Identifica los major numbers que corresponden a tus discos.

> **Pregunta de reflexión**: si tuvieras dos discos SATA y desconectaras el primero, ¿`/dev/sdb` pasaría a ser `/dev/sda`? ¿Cómo protege UUID contra este problema?

### Ejercicio 2: Loop device como disco virtual

1. Crea un archivo de 500 MiB con `dd`:
   ```bash
   dd if=/dev/zero of=/tmp/practice-disk.img bs=1M count=500
   ```
2. Asócialo a un loop device con `losetup -f`.
3. Verifica con `lsblk` que aparece. ¿Qué major:minor tiene?
4. Ejecuta `sudo blkid /dev/loop0`. ¿Muestra algo? ¿Por qué no?
5. Ejecuta `sudo file -s /dev/loop0`. ¿Qué dice?
6. Desasocia el loop device y elimina el archivo.

> **Pregunta de reflexión**: `blkid` no muestra nada para un dispositivo vacío porque no encuentra firmas de filesystem. ¿Qué tendrías que hacer para que `blkid` devuelva un resultado? (Pista: no basta con particionar — necesitas algo más.)

### Ejercicio 3: Comparar host y VM

Si tienes una VM corriendo con QEMU/KVM:

1. En el host, ejecuta `virsh domblklist <tu-vm>` para ver qué imágenes de disco usa.
2. Ejecuta `qemu-img info` sobre la imagen principal. Anota el tamaño virtual y el real.
3. Dentro de la VM, ejecuta `lsblk` y `lsblk -f`.
4. Compara: ¿el tamaño que la VM ve coincide con el tamaño virtual de `qemu-img info`?
5. ¿Los major:minor numbers dentro de la VM son iguales a los del host? ¿Por qué tienen que ser diferentes?
6. Si la VM no tiene un segundo disco, añade uno desde el host:
   ```bash
   qemu-img create -f qcow2 /var/lib/libvirt/images/test-extra.qcow2 2G
   virsh attach-disk <tu-vm> /var/lib/libvirt/images/test-extra.qcow2 vdb \
     --driver qemu --subdriver qcow2 --persistent
   ```
   Verifica dentro de la VM que `vdb` aparece en `lsblk`.

> **Pregunta de reflexión**: el disco de 2 GiB que acabas de crear con qcow2, ¿cuánto espacio ocupa realmente en el host? ¿Cuándo crecerá hasta los 2 GiB reales?
