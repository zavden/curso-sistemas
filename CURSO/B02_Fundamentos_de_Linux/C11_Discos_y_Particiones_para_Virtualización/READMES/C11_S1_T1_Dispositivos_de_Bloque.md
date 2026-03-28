# Dispositivos de Bloque

> **Sin erratas detectadas** en el archivo fuente `README.md`.

---

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

Ejecuta en tu sistema (host o VM de trabajo):

```bash
lsblk
lsblk -f
```

Dibuja en papel el árbol de discos → particiones → LVM (si hay). Anota qué filesystem tiene cada partición montada.

**Predicción antes de ejecutar:**

<details><summary>Si tu sistema es un host Fedora con SSD SATA y LVM, ¿qué prefijo tendrán los discos? ¿Cuántos niveles de profundidad tendrá el árbol?</summary>

- Los discos tendrán prefijo `sd*` (SATA) o `nvme*` (NVMe según tu hardware).
- El árbol tendrá 3 niveles si usas LVM: disco (`sda`) → partición (`sda3`) → volúmenes lógicos (`fedora-root`, `fedora-swap`, `fedora-home`).
- `sda1` (EFI) será `vfat`, `sda2` (`/boot`) será `ext4`, y los volúmenes LVM serán `xfs` o `ext4` según la distribución.
- Si estás dentro de una VM, los discos serán `vda` con major 252 (virtio).
</details>

---

### Ejercicio 2: blkid vs lsblk -f

Compara la salida de ambos comandos:

```bash
lsblk -f
sudo blkid
```

Identifica las diferencias: ¿qué columnas muestra `blkid` que `lsblk -f` no? ¿Y viceversa?

**Predicción antes de ejecutar:**

<details><summary>¿Qué campo exclusivo de blkid necesitarías para configurar un bootloader UEFI? ¿Cuál es más útil para un script que busca un disco por su UUID?</summary>

- `blkid` muestra **PARTUUID** y **BLOCK_SIZE**, que `lsblk -f` no muestra. PARTUUID es el que algunos bootloaders UEFI (como systemd-boot) usan en su configuración.
- `lsblk -f` muestra el **árbol jerárquico** y el **punto de montaje**, que `blkid` no muestra.
- Para un script que busca por UUID, `blkid -U "<uuid>"` devuelve directamente el path del dispositivo — más conveniente que parsear `lsblk -f`.
</details>

---

### Ejercicio 3: Explorar /dev/disk/by-*

Lista los contenidos de cada directorio `by-*`:

```bash
ls -la /dev/disk/by-uuid/
ls -la /dev/disk/by-id/
ls -la /dev/disk/by-path/
ls -la /dev/disk/by-partuuid/ 2>/dev/null
ls -la /dev/disk/by-label/ 2>/dev/null
```

Verifica que cada symlink apunta al dispositivo correcto cruzando con `lsblk -f`.

**Predicción antes de ejecutar:**

<details><summary>¿El directorio by-label existirá si ninguna partición tiene label? ¿Cuántos symlinks habrá en by-uuid?</summary>

- `by-label` **no existirá** (o estará vacío) si ningún filesystem tiene label asignado. Por eso el `2>/dev/null` en el comando.
- `by-uuid` tendrá **un symlink por cada partición que tenga filesystem** (no por particiones vacías como la de BIOS boot de 1 MiB).
- `by-id` incluye entradas tanto para el disco entero como para cada partición, con el serial del hardware — tendrá más symlinks que `by-uuid`.
</details>

---

### Ejercicio 4: Loop device como disco virtual

Crea un loop device de 500 MiB y explóralo:

```bash
# Crear archivo
dd if=/dev/zero of=/tmp/practice-disk.img bs=1M count=500

# Asociar
sudo losetup -f /tmp/practice-disk.img

# Verificar
losetup -l
lsblk | grep loop
sudo blkid /dev/loop0
sudo file -s /dev/loop0
```

**Predicción antes de ejecutar:**

<details><summary>¿Qué mostrará blkid para un loop device vacío? ¿Y file -s?</summary>

- `blkid` no mostrará nada (sin output) porque el dispositivo está lleno de ceros — no hay firmas de filesystem ni tabla de particiones.
- `file -s /dev/loop0` mostrará `data` (datos sin formato reconocido) porque no contiene ninguna estructura.
- Para que `blkid` devuelva algo, necesitarías: (a) crear una tabla de particiones con `fdisk`, o (b) crear un filesystem con `mkfs` directamente sobre el dispositivo.
- El major:minor será `7:X` (major 7 = loop driver).

Limpieza: `sudo losetup -d /dev/loop0 && rm /tmp/practice-disk.img`
</details>

---

### Ejercicio 5: Major y minor numbers

Examina los major:minor de tus dispositivos y correlaciónalos con los drivers del kernel:

```bash
# Ver major:minor de todos los bloques
lsblk -o NAME,MAJ:MIN,TYPE

# Ver drivers registrados
cat /proc/devices
```

Busca la sección "Block devices" en `/proc/devices` e identifica qué major corresponde a cada driver.

**Predicción antes de ejecutar:**

<details><summary>Si tienes un disco SATA (sda) con 3 particiones, ¿cuáles serán los minor numbers? ¿Por qué sdb empieza en minor 16?</summary>

- `sda` = (8, 0), `sda1` = (8, 1), `sda2` = (8, 2), `sda3` = (8, 3).
- `sdb` empieza en minor **16** porque el esquema reserva 16 minors por disco: minor 0 para el disco y 1-15 para hasta 15 particiones. Así, `sdb` = (8, 16), `sdb1` = (8, 17).
- En `/proc/devices`, el major 8 aparece como `sd` y el major 253 como `device-mapper` (LVM).
</details>

---

### Ejercicio 6: Comparar host y VM

Si tienes una VM corriendo, ejecuta `lsblk` en ambos entornos:

```bash
# En el host:
lsblk -o NAME,MAJ:MIN,SIZE,TYPE
virsh domblklist debian-lab
qemu-img info /var/lib/libvirt/images/debian-lab.qcow2

# Dentro de la VM:
lsblk -o NAME,MAJ:MIN,SIZE,TYPE
```

Compara los major:minor, nombres de dispositivo, y tamaños.

**Predicción antes de ejecutar:**

<details><summary>¿Los major:minor del host y la VM serán iguales? ¿El tamaño virtual de qemu-img coincidirá con lo que ve la VM?</summary>

- Los major:minor son **completamente independientes**. El host tiene major 8 (sd) o 259 (nvme), la VM tiene major 252 (virtio). Son kernels distintos con sus propios registros de drivers.
- El tamaño que la VM ve con `lsblk` coincide exactamente con el **virtual size** de `qemu-img info` (ej: 20 GiB).
- El **disk size** (tamaño real en el host) será menor si el formato es qcow2 (thin provisioning): solo ocupa lo que la VM ha escrito realmente.
</details>

---

### Ejercicio 7: Simular detección desordenada de discos

Entiende por qué UUID es necesario:

```bash
# Crear dos "discos" con loop devices
dd if=/dev/zero of=/tmp/disk-a.img bs=1M count=100
dd if=/dev/zero of=/tmp/disk-b.img bs=1M count=200

sudo losetup /dev/loop10 /tmp/disk-a.img
sudo losetup /dev/loop11 /tmp/disk-b.img

# Verificar orden
lsblk /dev/loop10 /dev/loop11

# Ahora desasociar y asociar en orden inverso
sudo losetup -d /dev/loop10
sudo losetup -d /dev/loop11
sudo losetup /dev/loop10 /tmp/disk-b.img   # ← ahora el de 200M
sudo losetup /dev/loop11 /tmp/disk-a.img   # ← ahora el de 100M

# Verificar
lsblk /dev/loop10 /dev/loop11
```

**Predicción antes de ejecutar:**

<details><summary>¿Qué tamaño muestra loop10 después de invertir la asociación? ¿Esto podría pasar con discos reales?</summary>

- Después de invertir, `loop10` mostrará **200 MiB** (antes era 100 MiB) y `loop11` mostrará **100 MiB**. El nombre del dispositivo no está ligado al contenido.
- **Sí, esto pasa con discos reales**: si desconectas y reconectas discos SATA/USB, el kernel puede asignar `/dev/sda` al que antes era `/dev/sdb`. Por eso fstab usa UUID, no nombres de dispositivo.

Limpieza: `sudo losetup -d /dev/loop10 /dev/loop11 && rm /tmp/disk-a.img /tmp/disk-b.img`
</details>

---

### Ejercicio 8: Crear un loop device con tabla de particiones

Extiende el ejercicio 4 creando particiones (anticipo de T02):

```bash
# Crear disco de 1 GiB
dd if=/dev/zero of=/tmp/disk-partitioned.img bs=1M count=1024
sudo losetup -f /tmp/disk-partitioned.img
# Anotar qué loop device se asignó (ej: /dev/loop0)

# Crear una tabla de particiones GPT con una partición
sudo fdisk /dev/loop0 <<'FDISK'
g
n
1

+500M
w
FDISK

# Re-leer particiones
sudo partprobe /dev/loop0

# Verificar
lsblk /dev/loop0
sudo blkid /dev/loop0*
```

**Predicción antes de ejecutar:**

<details><summary>Después de fdisk + partprobe, ¿qué dispositivos nuevos aparecen? ¿blkid mostrará UUID para la partición?</summary>

- Aparecerá `/dev/loop0p1` (la partición creada de 500 MiB).
- `blkid /dev/loop0` mostrará **PTUUID** y **PTTYPE="gpt"** (identifica la tabla de particiones, no un filesystem).
- `blkid /dev/loop0p1` mostrará solo **PARTUUID** (UUID de la partición GPT) pero **no UUID** de filesystem, porque aún no hemos formateado con `mkfs`. El UUID de filesystem se crea en S02.

Limpieza: `sudo losetup -d /dev/loop0 && rm /tmp/disk-partitioned.img`
</details>

---

### Ejercicio 9: Ver imágenes de disco de VMs

Desde el host, examina las imágenes de disco de tus VMs:

```bash
# Listar imágenes en el pool por defecto
ls -lh /var/lib/libvirt/images/

# Para cada VM activa, ver sus discos
virsh list --name | while read vm; do
  [ -z "$vm" ] && continue
  echo "=== $vm ==="
  virsh domblklist "$vm" --details
done

# Examinar una imagen específica
qemu-img info /var/lib/libvirt/images/debian-lab.qcow2
```

**Predicción antes de ejecutar:**

<details><summary>Si una VM tiene un disco de 20 GiB virtual en qcow2 y solo ha usado 3 GiB, ¿qué mostrará ls -lh vs qemu-img info?</summary>

- `ls -lh` mostrará el tamaño real en el host: aproximadamente **3-4 GiB** (lo que la VM ha escrito más overhead de metadatos qcow2).
- `qemu-img info` mostrará dos valores: **virtual size: 20 GiB** (lo que la VM ve) y **disk size: ~3 GiB** (lo que ocupa realmente).
- `domblklist --details` muestra el tipo (`file`), el dispositivo (`disk`), y el formato (qcow2/raw) además del path.
</details>

---

### Ejercicio 10: lsblk, df y blkid — cuándo usar cada uno

Ejecuta los tres comandos y analiza qué pregunta responde cada uno:

```bash
lsblk
df -h
sudo blkid
```

Para cada línea de salida, anota si la información podría obtenerse con los otros dos comandos.

**Predicción antes de ejecutar:**

<details><summary>¿Cuál de los tres comandos mostrará un disco recién añadido sin particiones ni filesystem? ¿Cuál mostrará el espacio libre?</summary>

- **Disco vacío recién añadido**: solo `lsblk` lo mostrará (como `vdb` con TYPE=disk, sin hijos). `df` no lo ve porque no tiene filesystem montado. `blkid` no lo ve porque no tiene firmas.
- **Espacio libre/usado**: solo `df -h` muestra espacio disponible y porcentaje de uso en cada punto de montaje. `lsblk` muestra tamaño total pero no espacio libre. `blkid` no muestra tamaños.
- **UUID**: `blkid` y `lsblk -f` lo muestran; `df` no.
- **PARTUUID**: solo `blkid`.
- **Punto de montaje**: `lsblk` y `df`; `blkid` no.

Resumen: `lsblk` = inventario completo, `df` = capacidad de montados, `blkid` = identificación profunda.
</details>
