# Información del dispositivo

## Índice

1. [Herramientas de inspección](#1-herramientas-de-inspección)
2. [lsblk: árbol de dispositivos](#2-lsblk-árbol-de-dispositivos)
3. [blkid: identificadores de filesystems](#3-blkid-identificadores-de-filesystems)
4. [fdisk -l: tabla de particiones](#4-fdisk--l-tabla-de-particiones)
5. [udevadm: información completa del kernel](#5-udevadm-información-completa-del-kernel)
6. [/proc y /sys: información directa del kernel](#6-proc-y-sys-información-directa-del-kernel)
7. [Combinando herramientas](#7-combinando-herramientas)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. Herramientas de inspección

Linux ofrece varias herramientas para inspeccionar dispositivos de bloque.
Cada una muestra información diferente:

```
┌─────────────────────────────────────────────────────────────┐
│              ¿QUÉ HERRAMIENTA USAR?                        │
│                                                             │
│  "¿Qué discos y particiones tengo?"     →  lsblk            │
│  "¿Qué UUID/label tiene este fs?"       →  blkid            │
│  "¿Cómo está particionado este disco?"  →  fdisk -l          │
│  "¿Qué sabe el kernel de este device?"  →  udevadm info     │
│  "¿Cuánto espacio tengo montado?"       →  df -h             │
│  "¿Qué I/O está generando?"            →  iostat            │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. lsblk: árbol de dispositivos

### 2.1. Uso básico

`lsblk` (**l**i**s**t **bl**oc**k** devices) es la herramienta más usada. Muestra
un árbol jerárquico de todos los dispositivos de bloque:

```bash
lsblk
```

```
NAME   MAJ:MIN RM  SIZE RO TYPE MOUNTPOINTS
vda      252:0    0   10G  0 disk
├─vda1   252:1    0    9G  0 part /
├─vda2   252:2    0    1K  0 part
└─vda5   252:5    0  975M  0 part [SWAP]
vdb      252:16   0    2G  0 disk
vdc      252:32   0    2G  0 disk
vdd      252:48   0    2G  0 disk
vde      252:64   0    2G  0 disk
```

### 2.2. Columnas del output

| Columna | Significado |
|---------|-------------|
| NAME | Nombre del dispositivo (sin `/dev/`) |
| MAJ:MIN | Major:minor number (identificador del kernel) |
| RM | Removible (1 = sí, 0 = no) |
| SIZE | Tamaño del dispositivo |
| RO | Read-only (1 = sí, 0 = no) |
| TYPE | Tipo: `disk`, `part`, `loop`, `lvm`, `raid1`, etc. |
| MOUNTPOINTS | Dónde está montado (vacío si no lo está) |

### 2.3. Columnas adicionales con -o

`lsblk` puede mostrar muchas más columnas. Selecciónalas con `-o`:

```bash
# Ver UUID, filesystem y label
lsblk -o NAME,SIZE,FSTYPE,UUID,LABEL,MOUNTPOINTS
```

```
NAME   SIZE FSTYPE UUID                                 LABEL MOUNTPOINTS
vda     10G
├─vda1   9G ext4   a1b2c3d4-e5f6-7890-abcd-ef12345678   root  /
├─vda2   1K
└─vda5 975M swap   11223344-5566-7788-99aa-bbccddeeff00        [SWAP]
vdb      2G
vdc      2G
vdd      2G
vde      2G
```

Los discos `vdb`-`vde` no tienen FSTYPE ni UUID porque están vacíos — no tienen
filesystem.

### 2.4. Columnas útiles

```bash
# Todas las columnas disponibles
lsblk --help | grep "Available"
# O:
lsblk -h
```

Las más útiles:

| Columna | Muestra | Ejemplo |
|---------|---------|---------|
| `FSTYPE` | Tipo de filesystem | ext4, xfs, swap |
| `UUID` | UUID del filesystem | a1b2c3d4-... |
| `LABEL` | Etiqueta del filesystem | root, datos |
| `MODEL` | Modelo del hardware | VBOX_HARDDISK |
| `SERIAL` | Número de serie | S3Y9NB0K123 |
| `TRAN` | Tipo de transporte | sata, usb, nvme, virtio |
| `ROTA` | Rotacional (1=HDD, 0=SSD) | 0 |
| `DISC-GRAN` | Granularidad de discard/TRIM | 512B |
| `SCHED` | Scheduler de I/O | mq-deadline, none |
| `PHY-SEC` | Tamaño del sector físico | 512, 4096 |
| `LOG-SEC` | Tamaño del sector lógico | 512 |

### 2.5. Formatos de salida

```bash
# Lista sin árbol (útil para scripting)
lsblk -ln
```

```
vda    252:0    0   10G  0 disk
vda1   252:1    0    9G  0 part /
vda2   252:2    0    1K  0 part
vda5   252:5    0  975M  0 part [SWAP]
vdb    252:16   0    2G  0 disk
```

```bash
# Solo discos (sin particiones)
lsblk -d
```

```
NAME MAJ:MIN RM  SIZE RO TYPE MOUNTPOINTS
vda    252:0    0   10G  0 disk
vdb    252:16   0    2G  0 disk
vdc    252:32   0    2G  0 disk
vdd    252:48   0    2G  0 disk
vde    252:64   0    2G  0 disk
```

```bash
# Output en JSON (para scripts)
lsblk -J
```

```bash
# Output en pares clave=valor
lsblk -P -o NAME,SIZE,TYPE
# NAME="vda" SIZE="10G" TYPE="disk"
# NAME="vda1" SIZE="9G" TYPE="part"
```

### 2.6. Inspeccionar un dispositivo específico

```bash
# Solo un disco y sus particiones
lsblk /dev/vda

# Solo un disco sin hijos
lsblk -d /dev/vda
```

---

## 3. blkid: identificadores de filesystems

### 3.1. Uso básico

`blkid` (**bl**ock **id**entifier) muestra los identificadores de los filesystems:
UUID, tipo, label, etc. Requiere `sudo` para ver todos los dispositivos:

```bash
sudo blkid
```

```
/dev/vda1: UUID="a1b2c3d4-e5f6-7890-abcd-ef1234567890" BLOCK_SIZE="4096" TYPE="ext4" PARTUUID="12345678-01"
/dev/vda5: UUID="11223344-5566-7788-99aa-bbccddeeff00" TYPE="swap" PARTUUID="12345678-05"
```

**Predicción**: `vdb`-`vde` no aparecen porque no tienen filesystem. `blkid`
solo muestra dispositivos con un filesystem o firma reconocible.

### 3.2. Campos del output

| Campo | Significado |
|-------|-------------|
| `UUID` | Identificador único del filesystem (generado por `mkfs`) |
| `TYPE` | Tipo de filesystem (ext4, xfs, swap, vfat, etc.) |
| `LABEL` | Etiqueta asignada al filesystem (opcional) |
| `PARTUUID` | UUID de la partición (viene de la tabla GPT/MBR) |
| `PARTLABEL` | Etiqueta de la partición (solo GPT) |
| `BLOCK_SIZE` | Tamaño de bloque del filesystem |

### 3.3. Diferencia entre UUID y PARTUUID

```
┌──────────────────────────────────────────────────────────┐
│  UUID vs PARTUUID                                        │
│                                                          │
│  PARTUUID:                                               │
│    - Viene de la tabla de particiones (GPT o MBR)         │
│    - Existe aunque no haya filesystem                    │
│    - Cambia si reparticionas                             │
│    - No cambia si formateas                              │
│                                                          │
│  UUID:                                                   │
│    - Viene del filesystem (generado por mkfs)             │
│    - Solo existe si hay un filesystem                    │
│    - Cambia si formateas (mkfs genera uno nuevo)          │
│    - No cambia si reparticionas (no tocas el fs)          │
│                                                          │
│  Para /etc/fstab: usar UUID es lo más común               │
│  Para bootloader: PARTUUID a veces es necesario           │
└──────────────────────────────────────────────────────────┘
```

### 3.4. Consultar un dispositivo específico

```bash
# Un dispositivo concreto
sudo blkid /dev/vda1
# /dev/vda1: UUID="a1b2c3d4-..." TYPE="ext4" PARTUUID="12345678-01"

# Solo el UUID (útil para scripts)
sudo blkid -s UUID -o value /dev/vda1
# a1b2c3d4-e5f6-7890-abcd-ef1234567890

# Solo el tipo de filesystem
sudo blkid -s TYPE -o value /dev/vda1
# ext4
```

### 3.5. Buscar por UUID

```bash
# ¿Qué dispositivo tiene este UUID?
sudo blkid -U "a1b2c3d4-e5f6-7890-abcd-ef1234567890"
# /dev/vda1
```

Esto es exactamente lo que hace el kernel al montar particiones de `/etc/fstab`
referenciadas por UUID.

---

## 4. fdisk -l: tabla de particiones

### 4.1. Uso básico

`fdisk -l` muestra la tabla de particiones de un disco. A diferencia de `lsblk`,
muestra detalles de bajo nivel: sectores, tipo de partición, flags:

```bash
sudo fdisk -l /dev/vda
```

```
Disk /dev/vda: 10 GiB, 10737418240 bytes, 20971520 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / 512 bytes
Disklabel type: dos
Disk identifier: 0x12345678

Device     Boot   Start      End  Sectors  Size Id  Type
/dev/vda1  *       2048 18872319 18870272    9G 83  Linux
/dev/vda2      18874366 20969471  2095106 1023M  5  Extended
/dev/vda5      18874368 20969471  2095104 1023M 82  Linux swap / Solaris
```

### 4.2. Campos del output

**Cabecera del disco**:

| Campo | Significado |
|-------|-------------|
| Disk size | Tamaño total del disco |
| Sectors | Número total de sectores |
| Sector size | Tamaño del sector (512B o 4096B) |
| Disklabel type | Tipo de tabla de particiones: `dos` (MBR) o `gpt` |
| Disk identifier | ID único del disco |

**Tabla de particiones**:

| Campo | Significado |
|-------|-------------|
| Device | Nombre del dispositivo de la partición |
| Boot | `*` = partición de arranque |
| Start/End | Sector de inicio y fin |
| Sectors | Número de sectores (tamaño en sectores) |
| Size | Tamaño en formato legible |
| Id | Código del tipo de partición (hex) |
| Type | Descripción del tipo de partición |

### 4.3. Tipos de partición comunes (Id)

| Id | Tipo | Uso |
|----|------|-----|
| 83 | Linux | Partición Linux estándar |
| 82 | Linux swap | Partición de swap |
| 5 | Extended | Contenedor para particiones lógicas |
| 8e | Linux LVM | Physical Volume para LVM |
| fd | Linux RAID autodetect | Partición para mdadm |
| ef | EFI System | Partición EFI (FAT32) |
| 7 | HPFS/NTFS | Partición Windows |

### 4.4. Disco vacío vs particionado

```bash
# Disco sin particiones (los discos extra del lab)
sudo fdisk -l /dev/vdb
```

```
Disk /dev/vdb: 2 GiB, 2147483648 bytes, 4194304 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / 512 bytes
```

No hay tabla de particiones — el disco está completamente vacío. No tiene
`Disklabel type` ni tabla de particiones.

### 4.5. Listar todos los discos

```bash
# Todos los discos del sistema
sudo fdisk -l
```

Esto muestra la información de cada `/dev/vd*`, `/dev/sd*`, etc. Útil para
una vista rápida de todo el almacenamiento.

---

## 5. udevadm: información completa del kernel

### 5.1. Qué es udevadm

`udevadm` es la herramienta para interactuar con **udev**, el gestor de
dispositivos del kernel. Muestra toda la información que el kernel y udev
conocen sobre un dispositivo:

```bash
udevadm info /dev/vda
```

```
P: /devices/pci0000:00/0000:00:05.0/virtio2/block/vda
N: vda
L: 0
S: disk/by-path/pci-0000:00:05.0
S: disk/by-path/virtio-pci-0000:00:05.0
E: DEVPATH=/devices/pci0000:00/0000:00:05.0/virtio2/block/vda
E: DEVNAME=/dev/vda
E: DEVTYPE=disk
E: DISKSEQ=9
E: MAJOR=252
E: MINOR=0
E: SUBSYSTEM=block
E: ID_PATH=pci-0000:00:05.0
E: ID_PATH_TAG=pci-0000_00_05_0
E: ID_PART_TABLE_TYPE=dos
E: ID_PART_TABLE_UUID=12345678
```

### 5.2. Campos clave

| Prefijo | Significado |
|---------|-------------|
| `P:` | Path en sysfs (`/sys/...`) |
| `N:` | Nombre del device en `/dev/` |
| `S:` | Symlinks creados por udev (en `/dev/disk/...`) |
| `E:` | Variables de entorno (propiedades del dispositivo) |

### 5.3. Propiedades de entorno útiles

```bash
# Ver solo las propiedades (formato más limpio)
udevadm info --query=property /dev/vda
```

| Propiedad | Ejemplo | Significado |
|-----------|---------|-------------|
| `DEVNAME` | /dev/vda | Nombre del dispositivo |
| `DEVTYPE` | disk / partition | Disco o partición |
| `MAJOR` / `MINOR` | 252 / 0 | Major:minor number |
| `SUBSYSTEM` | block | Subsistema del kernel |
| `ID_PATH` | pci-0000:00:05.0 | Ruta del bus |
| `ID_SERIAL` | serial_number | Serial del disco |
| `ID_MODEL` | QEMU_HARDDISK | Modelo del disco |
| `ID_FS_TYPE` | ext4 | Tipo de filesystem |
| `ID_FS_UUID` | a1b2c3d4-... | UUID del filesystem |
| `ID_PART_TABLE_TYPE` | dos / gpt | Tipo de tabla de particiones |

### 5.4. Información de una partición

```bash
udevadm info /dev/vda1
```

```
P: /devices/pci0000:00/0000:00:05.0/virtio2/block/vda/vda1
N: vda1
S: disk/by-uuid/a1b2c3d4-e5f6-7890-abcd-ef1234567890
S: disk/by-path/pci-0000:00:05.0-part1
E: DEVNAME=/dev/vda1
E: DEVTYPE=partition
E: ID_FS_TYPE=ext4
E: ID_FS_UUID=a1b2c3d4-e5f6-7890-abcd-ef1234567890
E: ID_FS_USAGE=filesystem
E: ID_PART_ENTRY_NUMBER=1
E: ID_PART_ENTRY_SIZE=18870272
E: ID_PART_ENTRY_TYPE=0x83
```

Aquí puedes ver el UUID, el tipo de filesystem, y el tipo de partición (0x83 =
Linux) — todo en un solo comando.

### 5.5. Monitorear eventos de dispositivos

```bash
# Escuchar eventos en tiempo real (útil para debugging)
udevadm monitor
```

Si en otra terminal haces `virsh attach-disk` (hot-plug), verás los eventos
que genera udev al detectar el nuevo disco:

```
KERNEL[12345.678] add      /devices/.../block/vdf (block)
UDEV  [12345.690] add      /devices/.../block/vdf (block)
```

Presiona `Ctrl+C` para salir.

### 5.6. Recorrer la jerarquía del dispositivo

```bash
# Ver toda la cadena del dispositivo: device → bus → controlador
udevadm info --attribute-walk /dev/vda
```

Esto muestra el dispositivo y todos sus ancestros en el árbol de sysfs. Útil
para escribir reglas udev (T03), pero por ahora es informativo.

---

## 6. /proc y /sys: información directa del kernel

### 6.1. /proc/partitions

```bash
cat /proc/partitions
```

```
major minor  #blocks  name
 252        0   10485760 vda
 252        1    9435136 vda1
 252        2          1 vda2
 252        5    1047552 vda5
 252       16    2097152 vdb
 252       32    2097152 vdc
 252       48    2097152 vdd
 252       64    2097152 vde
```

Esta es la fuente más directa — el kernel exporta su lista de dispositivos de
bloque aquí. `lsblk` lee de aquí (entre otros sitios).

### 6.2. /sys/block/

Cada dispositivo de bloque tiene un directorio en `/sys/block/`:

```bash
ls /sys/block/
# vda  vdb  vdc  vdd  vde
```

Cada directorio contiene archivos con información del dispositivo:

```bash
# Tamaño en sectores de 512 bytes
cat /sys/block/vdb/size
# 4194304    (4194304 × 512 = 2GB)

# ¿Es rotacional? (1=HDD, 0=SSD/virtual)
cat /sys/block/vdb/queue/rotational
# 1    (el kernel no sabe que es virtual, asume rotacional)

# Scheduler de I/O
cat /sys/block/vdb/queue/scheduler
# [mq-deadline] none

# Tamaño de sector lógico
cat /sys/block/vdb/queue/logical_block_size
# 512

# Tamaño de sector físico
cat /sys/block/vdb/queue/physical_block_size
# 512
```

### 6.3. /proc/diskstats

Estadísticas de I/O por dispositivo:

```bash
cat /proc/diskstats | grep vda
```

```
 252    0 vda 1234 567 89012 3456 789 012 34567 890 0 1234 5678 0 0 0 0
```

Los campos son (en orden): reads completed, reads merged, sectors read, ms
reading, writes completed, writes merged, sectors written, ms writing, etc.
Herramientas como `iostat` parsean estos datos en un formato legible.

---

## 7. Combinando herramientas

### 7.1. Escenarios de uso

```
Escenario: "Acabo de adjuntar discos a mi VM, ¿qué tengo?"

  Paso 1: lsblk                     → ver todos los discos y particiones
  Paso 2: lsblk -o NAME,SIZE,FSTYPE → ver cuáles tienen filesystem
  Paso 3: sudo blkid                → ver UUIDs de los que sí tienen fs
```

```
Escenario: "Necesito el UUID de /dev/vda1 para /etc/fstab"

  Opción A: sudo blkid /dev/vda1
  Opción B: lsblk -o UUID /dev/vda1
  Opción C: ls -la /dev/disk/by-uuid/ | grep vda1
```

```
Escenario: "¿Qué tipo de disco es /dev/sda? ¿HDD o SSD?"

  Opción A: lsblk -o NAME,ROTA /dev/sda    (ROTA=1 → HDD, 0 → SSD)
  Opción B: cat /sys/block/sda/queue/rotational
```

```
Escenario: "Quiero saber todo sobre este disco"

  udevadm info /dev/vda        → propiedades del kernel
  sudo fdisk -l /dev/vda       → tabla de particiones detallada
  sudo hdparm -I /dev/sda      → info del hardware (solo discos reales)
```

### 7.2. lsblk personalizado para labs

Un comando útil para ver el estado de los discos de lab de un vistazo:

```bash
lsblk -o NAME,SIZE,TYPE,FSTYPE,MOUNTPOINTS,UUID
```

```
NAME   SIZE TYPE FSTYPE MOUNTPOINTS UUID
vda     10G disk
├─vda1   9G part ext4   /           a1b2c3d4-...
├─vda2   1K part
└─vda5 975M part swap   [SWAP]      11223344-...
vdb      2G disk                                    ← vacío, listo para lab
vdc      2G disk                                    ← vacío
vdd      2G disk                                    ← vacío
vde      2G disk                                    ← vacío
```

Los discos sin FSTYPE ni UUID son los que están disponibles para practicar.

---

## 8. Errores comunes

### Error 1: ejecutar blkid sin sudo

```bash
# ✗ Sin sudo — puede no mostrar todos los dispositivos
blkid
# (output incompleto o vacío)

# ✓ Con sudo
sudo blkid
# /dev/vda1: UUID="a1b2c3d4-..." TYPE="ext4" ...
```

`blkid` necesita permisos de lectura en los dispositivos. Sin `sudo` puede
no mostrar nada o mostrar solo los filesystems ya montados.

### Error 2: confundir lsblk con df

```bash
# lsblk: muestra DISPOSITIVOS (discos y particiones)
#   → incluye los no montados
#   → muestra tamaño total del dispositivo

# df: muestra FILESYSTEMS MONTADOS
#   → solo los montados
#   → muestra espacio usado/disponible

# ✗ "¿Cuánto espacio libre tengo?" → NO uses lsblk
lsblk   # muestra tamaño del dispositivo, no el espacio libre

# ✓ Para espacio libre, usa df
df -h
```

### Error 3: buscar discos vacíos con blkid

```bash
# ✗ "¿Por qué vdb no aparece en blkid?"
sudo blkid
# Solo muestra vda1 y vda5 — ¡faltan vdb-vde!

# Esto es normal: blkid solo muestra dispositivos con filesystem
# Los discos vacíos no tienen filesystem → no aparecen

# ✓ Para ver TODOS los dispositivos (con y sin fs), usa lsblk
lsblk
```

### Error 4: confundir sector size lógico y físico

```bash
# Algunos discos modernos tienen:
#   Logical sector size:  512 bytes    (lo que el SO ve)
#   Physical sector size: 4096 bytes   (lo que el hardware usa)

# Esto se llama "512e" (emulación de 512)
# El disco internamente trabaja en 4K pero presenta sectores de 512

# Para verificar:
lsblk -o NAME,PHY-SEC,LOG-SEC /dev/vda
# NAME PHY-SEC LOG-SEC
# vda      512     512

# En VMs virtio ambos son 512. En discos reales pueden diferir.
```

### Error 5: usar fdisk -l sin especificar disco

```bash
# ✗ Sin dispositivo — muestra TODOS los discos (output largo)
sudo fdisk -l
# ... páginas de output ...

# ✓ Especifica el disco que te interesa
sudo fdisk -l /dev/vdb
```

---

## 9. Cheatsheet

```bash
# ── lsblk ────────────────────────────────────────────────────────
lsblk                                    # árbol de dispositivos
lsblk -d                                 # solo discos (sin particiones)
lsblk -f                                 # con filesystem, UUID, label
lsblk -o NAME,SIZE,TYPE,FSTYPE,UUID      # columnas personalizadas
lsblk -ln                                # lista plana (para scripts)
lsblk -J                                 # output JSON
lsblk -P                                 # output clave=valor
lsblk /dev/vda                           # solo un dispositivo

# ── blkid ────────────────────────────────────────────────────────
sudo blkid                               # todos los filesystems
sudo blkid /dev/vda1                     # un dispositivo específico
sudo blkid -s UUID -o value /dev/vda1    # solo el UUID
sudo blkid -s TYPE -o value /dev/vda1    # solo el tipo de fs
sudo blkid -U "uuid-aquí"               # buscar por UUID

# ── fdisk ────────────────────────────────────────────────────────
sudo fdisk -l /dev/vda                   # tabla de particiones
sudo fdisk -l                            # todos los discos

# ── udevadm ──────────────────────────────────────────────────────
udevadm info /dev/vda                    # info completa
udevadm info --query=property /dev/vda   # solo propiedades
udevadm info --query=symlink /dev/vda    # solo symlinks
udevadm monitor                          # eventos en tiempo real
udevadm info --attribute-walk /dev/vda   # jerarquía completa

# ── /proc y /sys ─────────────────────────────────────────────────
cat /proc/partitions                     # lista del kernel
cat /sys/block/vdb/size                  # tamaño en sectores
cat /sys/block/vdb/queue/rotational      # 1=HDD, 0=SSD
cat /sys/block/vdb/queue/scheduler       # scheduler de I/O
cat /proc/diskstats                      # estadísticas de I/O

# ── Combinaciones útiles ─────────────────────────────────────────
lsblk -o NAME,SIZE,TYPE,FSTYPE,MOUNTPOINTS,UUID    # vista de lab
lsblk -o NAME,ROTA,TRAN,MODEL                      # tipo de hardware
df -h                                               # espacio montado
```

---

## 10. Ejercicios

### Ejercicio 1: inspección completa de la VM

1. Ejecuta `lsblk` y anota cuántos discos y particiones tienes
2. Ejecuta `lsblk -o NAME,SIZE,TYPE,FSTYPE,UUID,MOUNTPOINTS` — identifica:
   - ¿Cuáles tienen filesystem?
   - ¿Cuáles están montados?
   - ¿Cuáles están vacíos?
3. Ejecuta `sudo blkid` — compara: ¿aparecen los mismos dispositivos que en
   `lsblk`? ¿Cuáles faltan y por qué?
4. Ejecuta `sudo fdisk -l /dev/vda` — identifica:
   - ¿Es MBR (`dos`) o GPT?
   - ¿Cuál es la partición de boot?
   - ¿Cuál es la partición extendida?

**Pregunta de reflexión**: `lsblk` muestra `vda2` con tamaño 1K y `lsblk -f`
no muestra filesystem para ella. `fdisk -l` la muestra como tipo "Extended".
¿Qué es una partición extendida y por qué ocupa solo 1K?

### Ejercicio 2: explorar udevadm

1. Ejecuta `udevadm info /dev/vda` y busca la propiedad `ID_PART_TABLE_TYPE`
2. Ejecuta `udevadm info /dev/vda1` y busca `ID_FS_TYPE` e `ID_FS_UUID`
3. Compara el `ID_FS_UUID` con la salida de `sudo blkid /dev/vda1` — ¿coinciden?
4. Ejecuta `udevadm info --query=symlink /dev/vda1` — ¿qué symlinks tiene?
5. Verifica que esos symlinks existen: `ls -la /dev/disk/by-uuid/`

**Pregunta de reflexión**: `udevadm info` muestra la misma información que
`blkid` pero con más detalle. ¿Por qué existen ambas herramientas? ¿Cuándo
usarías una u otra?

### Ejercicio 3: crear filesystem y observar cambios

1. Ejecuta `lsblk -f /dev/vdb` — confirma que está vacío (sin FSTYPE)
2. Ejecuta `sudo blkid /dev/vdb` — confirma que no aparece
3. Crea un filesystem: `sudo mkfs.ext4 -L test-disk /dev/vdb`
4. Repite los comandos 1 y 2 — ¿qué cambió?
5. Verifica que apareció en `/dev/disk/by-uuid/` y `/dev/disk/by-label/`
6. Limpia: `sudo wipefs -a /dev/vdb` (elimina la firma del filesystem)

**Pregunta de reflexión**: después de `mkfs.ext4`, ¿cambió el tamaño del
disco en `lsblk`? ¿Cambió el major:minor? ¿Qué fue exactamente lo que `mkfs`
escribió en el disco para que `blkid` ahora lo reconozca?
