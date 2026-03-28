# Nomenclatura de dispositivos de bloque

## Índice

1. [Qué es un dispositivo de bloque](#1-qué-es-un-dispositivo-de-bloque)
2. [El directorio /dev](#2-el-directorio-dev)
3. [Discos SCSI/SATA/USB: /dev/sdX](#3-discos-scsisatausb-devsdx)
4. [Discos NVMe: /dev/nvmeXnY](#4-discos-nvme-devnvmexny)
5. [Discos virtio: /dev/vdX](#5-discos-virtio-devvdx)
6. [Otros dispositivos de bloque](#6-otros-dispositivos-de-bloque)
7. [Particiones: la extensión numérica](#7-particiones-la-extensión-numérica)
8. [El problema del orden inestable](#8-el-problema-del-orden-inestable)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Qué es un dispositivo de bloque

Un dispositivo de bloque es cualquier dispositivo de almacenamiento que el kernel
expone como una secuencia de bloques de tamaño fijo (típicamente 512 bytes o
4096 bytes). Se lee y escribe en bloques, no byte a byte.

```
Dispositivos de bloque:              Dispositivos de carácter:
┌──────────────────────┐             ┌──────────────────────┐
│  Discos duros (HDD)  │             │  Terminales (/dev/tty)│
│  SSDs                │             │  /dev/null            │
│  USB drives          │             │  /dev/random          │
│  NVMe                │             │  Puertos serie        │
│  Discos virtuales    │             │                      │
│  Particiones         │             │  Acceso: byte a byte  │
│                      │             └──────────────────────┘
│  Acceso: por bloques │
│  (512B o 4096B)      │
└──────────────────────┘
```

En Linux, todo dispositivo de bloque aparece como un archivo especial en `/dev/`.
Puedes leer/escribir ese archivo como si fuera un archivo normal, pero en realidad
estás accediendo al hardware (o al dispositivo virtual).

---

## 2. El directorio /dev

`/dev/` contiene archivos especiales que representan dispositivos. No son archivos
regulares — son interfaces al kernel:

```bash
ls -la /dev/sda
# brw-rw---- 1 root disk 8, 0  ... /dev/sda
# ^
# b = block device (dispositivo de bloque)
```

El `b` al inicio indica dispositivo de bloque. Los números `8, 0` son el
**major** y **minor** number:

| Componente | Significado |
|------------|-------------|
| Major (8) | Identifica el driver del kernel (8 = SCSI/SATA) |
| Minor (0) | Identifica el dispositivo específico dentro del driver |

```bash
# Major:minor de dispositivos comunes
ls -la /dev/sda   # 8, 0   ← primer disco SCSI/SATA
ls -la /dev/sda1  # 8, 1   ← primera partición de sda
ls -la /dev/sdb   # 8, 16  ← segundo disco SCSI/SATA
ls -la /dev/vda   # 252, 0 ← primer disco virtio
```

No necesitas memorizar los major/minor, pero es útil saber que existen. Herramientas
como `lsblk` muestran esta información de forma legible.

---

## 3. Discos SCSI/SATA/USB: /dev/sdX

### 3.1. El prefijo sd

`sd` viene de **SCSI disk**. Históricamente era para discos SCSI, pero el kernel
Linux usa el mismo driver (`sd_mod`) para:

```
/dev/sdX se usa para:
  ├── Discos SATA (los más comunes en PCs)
  ├── Discos SCSI (servidores)
  ├── Discos SAS (servidores)
  ├── Discos USB (pendrives, discos externos)
  └── Algunos discos virtuales (no virtio)
```

### 3.2. La letra: orden de detección

La letra después de `sd` indica el **orden en que el kernel detectó el disco**:

```
/dev/sda  ──► primer disco detectado
/dev/sdb  ──► segundo disco detectado
/dev/sdc  ──► tercer disco detectado
...
/dev/sdz  ──► disco número 26
/dev/sdaa ──► disco número 27 (continúa con dos letras)
```

**Importante**: la letra **no** indica la posición física ni el tipo de conexión.
`sda` es simplemente "el primero que el kernel encontró al arrancar". Si cambias
el orden de los cables o conectas un USB antes de arrancar, las letras pueden
cambiar.

### 3.3. Ejemplo en un servidor típico

```
┌─────────────────────────────────────────────────┐
│  Servidor con 3 discos SATA + 1 USB             │
│                                                  │
│  Puerto SATA 0 ──► /dev/sda  (500GB HDD)       │
│  Puerto SATA 1 ──► /dev/sdb  (500GB HDD)       │
│  Puerto SATA 2 ──► /dev/sdc  (256GB SSD)       │
│  Puerto USB    ──► /dev/sdd  (16GB pendrive)    │
│                                                  │
│  ⚠ Si desconectas el USB y reconectas:          │
│    podría ser /dev/sdd... o /dev/sde            │
│    El nombre NO es estable                       │
└─────────────────────────────────────────────────┘
```

---

## 4. Discos NVMe: /dev/nvmeXnY

### 4.1. Nomenclatura NVMe

Los discos NVMe (Non-Volatile Memory Express) usan un esquema de nombres diferente
porque su arquitectura interna es diferente — soportan múltiples colas de I/O
en paralelo:

```
/dev/nvme0n1
       │  │
       │  └── n1 = namespace 1 (casi siempre n1)
       └───── 0  = controlador 0 (primer controlador NVMe)
```

### 4.2. Controladores y namespaces

```
┌────────────────────────────────────────────────────┐
│  NVMe: controladores y namespaces                  │
│                                                    │
│  nvme0 ──► Controlador 0 (primer SSD NVMe)         │
│    └── nvme0n1 ──► Namespace 1 (el disco)          │
│        ├── nvme0n1p1 ──► Partición 1               │
│        ├── nvme0n1p2 ──► Partición 2               │
│        └── nvme0n1p3 ──► Partición 3               │
│                                                    │
│  nvme1 ──► Controlador 1 (segundo SSD NVMe)        │
│    └── nvme1n1 ──► Namespace 1                     │
│        ├── nvme1n1p1 ──► Partición 1               │
│        └── nvme1n1p2 ──► Partición 2               │
└────────────────────────────────────────────────────┘
```

Un namespace es una subdivisión lógica del SSD NVMe. En la mayoría de los casos
solo hay un namespace (`n1`), pero los SSDs empresariales pueden tener varios.

### 4.3. Por qué NVMe no usa sdX

NVMe usa su propio driver (`nvme`) en vez del driver SCSI (`sd_mod`). Tiene su
propia cola de comandos optimizada para la baja latencia de los SSDs:

| Aspecto | SCSI/SATA (sdX) | NVMe (nvmeXnY) |
|---------|-----------------|-----------------|
| Driver | sd_mod | nvme |
| Colas de I/O | 1 | Múltiples (hasta 65535) |
| Protocolo | AHCI/SCSI | NVMe nativo |
| Particiones | /dev/sdXN | /dev/nvmeXnYpN |
| Rendimiento típico | ~550 MB/s (SATA) | ~3500+ MB/s |

### 4.4. NVMe en la práctica

```bash
# PC moderno con un SSD NVMe + un HDD SATA
lsblk
```

```
NAME        MAJ:MIN RM   SIZE RO TYPE MOUNTPOINTS
sda           8:0    0   1.0T  0 disk
├─sda1        8:1    0   512M  0 part /boot/efi
└─sda2        8:2    0 999.5G  0 part /data
nvme0n1     259:0    0 476.9G  0 disk
├─nvme0n1p1 259:1    0   512M  0 part /boot
├─nvme0n1p2 259:2    0    16G  0 part [SWAP]
└─nvme0n1p3 259:3    0 460.4G  0 part /
```

---

## 5. Discos virtio: /dev/vdX

### 5.1. El bus virtio

En nuestras VMs de QEMU/KVM, los discos usan el bus **virtio** — un bus
paravirtualizado diseñado específicamente para máquinas virtuales:

```
/dev/vdX
     │
     └── vd = virtio disk
```

La secuencia es igual que sdX: `vda`, `vdb`, `vdc`...

### 5.2. Por qué virtio y no sdX

```
┌────────────────────────────────────────────────────────┐
│  Emulación SCSI vs Paravirtualización virtio           │
│                                                        │
│  /dev/sdX (emulación):                                 │
│  ┌──────┐    ┌──────────┐    ┌──────────┐              │
│  │ VM   │───►│ Driver   │───►│ QEMU     │──► disco     │
│  │      │    │ SCSI     │    │ emula    │   real       │
│  │      │    │ (genérico│    │ contro-  │              │
│  │      │    │  lento)  │    │ lador    │              │
│  └──────┘    └──────────┘    └──────────┘              │
│                                                        │
│  /dev/vdX (paravirtualización):                        │
│  ┌──────┐    ┌──────────┐    ┌──────────┐              │
│  │ VM   │───►│ Driver   │───►│ QEMU     │──► disco     │
│  │      │    │ virtio   │    │ (path    │   real       │
│  │      │    │ (optimiz.│    │  directo)│              │
│  │      │    │  rápido) │    │          │              │
│  └──────┘    └──────────┘    └──────────┘              │
│                                                        │
│  virtio: menos overhead, más rendimiento               │
└────────────────────────────────────────────────────────┘
```

### 5.3. En nuestras VMs de lab

```bash
# Dentro de una VM del lab (con 4 discos extra de C00)
lsblk
```

```
NAME   MAJ:MIN RM  SIZE RO TYPE MOUNTPOINTS
vda      252:0    0   10G  0 disk          ← disco del SO (virtio)
├─vda1   252:1    0    9G  0 part /
├─vda2   252:2    0    1K  0 part
└─vda5   252:5    0  975M  0 part [SWAP]
vdb      252:16   0    2G  0 disk          ← disco extra 1
vdc      252:32   0    2G  0 disk          ← disco extra 2
vdd      252:48   0    2G  0 disk          ← disco extra 3
vde      252:64   0    2G  0 disk          ← disco extra 4
```

Estos son los discos que creaste con `qemu-img create` y adjuntaste con
`virsh attach-disk`. Dentro de la VM aparecen como `vdX` porque usamos
`bus='virtio'` (o `--driver qemu` en attach-disk).

---

## 6. Otros dispositivos de bloque

### 6.1. Tabla completa de nomenclaturas

| Prefijo | Tipo | Ejemplo | Dónde lo encuentras |
|---------|------|---------|---------------------|
| `sd` | SCSI/SATA/USB/SAS | `/dev/sda` | PCs, servidores, USB |
| `nvme` | NVMe | `/dev/nvme0n1` | PCs modernos, servidores |
| `vd` | Virtio | `/dev/vda` | VMs con KVM/QEMU |
| `xvd` | Xen virtual disk | `/dev/xvda` | VMs en Xen (AWS antiguo) |
| `hd` | IDE (legacy) | `/dev/hda` | Hardware muy antiguo |
| `loop` | Loop device | `/dev/loop0` | Archivos montados como disco |
| `md` | RAID (mdadm) | `/dev/md0` | Arrays RAID por software |
| `dm` | Device mapper | `/dev/dm-0` | LVM, LUKS, multipath |
| `sr` | CD/DVD | `/dev/sr0` | Unidades ópticas |

### 6.2. Loop devices

Los loop devices merecen mención especial — permiten tratar un archivo regular
como si fuera un disco:

```bash
# Crear un archivo de 100MB
dd if=/dev/zero of=disk.img bs=1M count=100

# Asociarlo a un loop device
sudo losetup /dev/loop0 disk.img

# Ahora /dev/loop0 se comporta como un disco
sudo fdisk /dev/loop0
sudo mkfs.ext4 /dev/loop0
sudo mount /dev/loop0 /mnt
```

Los veremos en detalle en S02/T04 (Loop devices).

### 6.3. Device mapper (dm)

El device mapper es una capa del kernel que crea dispositivos virtuales a partir
de otros. LVM y LUKS lo usan:

```
┌──────────────────────────────────────────────────┐
│  Device Mapper: capa intermedia                  │
│                                                  │
│  /dev/dm-0 ──► /dev/mapper/vg-lab-lv--data       │
│  /dev/dm-1 ──► /dev/mapper/vg-lab-lv--logs       │
│                                                  │
│  Los nombres en /dev/mapper/ son más legibles    │
│  que /dev/dm-N                                   │
└──────────────────────────────────────────────────┘
```

Verás device mapper cuando trabajes con LVM (C03) y LUKS (B11).

---

## 7. Particiones: la extensión numérica

### 7.1. Convención por tipo de dispositivo

Cada tipo de disco usa una convención diferente para nombrar sus particiones:

```
SCSI/SATA/USB (sdX):          NVMe (nvmeXnY):
  /dev/sda                     /dev/nvme0n1
  /dev/sda1  ← partición 1    /dev/nvme0n1p1  ← partición 1
  /dev/sda2  ← partición 2    /dev/nvme0n1p2  ← partición 2
  /dev/sda3  ← partición 3    /dev/nvme0n1p3  ← partición 3

Virtio (vdX):
  /dev/vda
  /dev/vda1  ← partición 1
  /dev/vda2  ← partición 2
```

La diferencia clave: NVMe usa `p` antes del número de partición (`nvme0n1p1`)
para evitar ambigüedad, ya que el nombre del disco ya termina en un número
(`nvme0n1`).

### 7.2. MBR: primarias, extendida y lógicas

En discos con tabla de particiones MBR (la veremos en S02), la numeración tiene
significado:

```
/dev/sda1  ──► Partición primaria 1     ┐
/dev/sda2  ──► Partición primaria 2     ├── máximo 4 primarias
/dev/sda3  ──► Partición primaria 3     │   (o 3 primarias + 1 extendida)
/dev/sda4  ──► Partición extendida      ┘
/dev/sda5  ──► Partición lógica 1       ┐
/dev/sda6  ──► Partición lógica 2       ├── dentro de la extendida
/dev/sda7  ──► Partición lógica 3       ┘   (empiezan en 5)
```

En discos GPT no hay esta limitación — todas son "primarias" y la numeración
es simplemente secuencial.

### 7.3. Ejemplo en nuestras VMs

```bash
# VM del lab: disco del SO con particiones MBR
lsblk /dev/vda
```

```
NAME   MAJ:MIN RM  SIZE RO TYPE MOUNTPOINTS
vda      252:0    0   10G  0 disk
├─vda1   252:1    0    9G  0 part /          ← primaria 1 (root)
├─vda2   252:2    0    1K  0 part            ← extendida
└─vda5   252:5    0  975M  0 part [SWAP]     ← lógica 1 (swap)
```

Nota que no hay `vda3` ni `vda4` — las particiones lógicas empiezan en 5.
`vda2` es la extendida (1K porque es solo un contenedor).

---

## 8. El problema del orden inestable

### 8.1. El problema

Los nombres `/dev/sdX` dependen del orden de detección del kernel. Este orden
puede cambiar entre reboots:

```
Boot 1:                           Boot 2 (cambió un cable):
  /dev/sda ──► disco 500GB          /dev/sda ──► disco 256GB  ← ¡cambió!
  /dev/sdb ──► disco 256GB          /dev/sdb ──► disco 500GB  ← ¡cambió!
```

Si `/etc/fstab` dice "monta `/dev/sda1` en `/`", después del cambio montaría
el disco equivocado. Esto puede causar pérdida de datos o un sistema que no
arranca.

### 8.2. La solución: identificadores persistentes

Linux ofrece nombres estables a través de `/dev/disk/`:

```bash
ls -la /dev/disk/
# by-id/      → por modelo y serial del hardware
# by-uuid/    → por UUID del filesystem
# by-label/   → por etiqueta del filesystem
# by-path/    → por la ruta del bus (PCI, USB)
# by-partuuid/→ por UUID de la partición (GPT)
```

```bash
# Ejemplo: ver los symlinks de un disco
ls -la /dev/disk/by-uuid/
# a1b2c3d4-... -> ../../sda1
# e5f6g7h8-... -> ../../sda2

ls -la /dev/disk/by-id/
# ata-Samsung_SSD_860_S3Y9NB0K123456-part1 -> ../../sda1
# usb-SanDisk_Cruzer_12345678-0:0 -> ../../sdb
```

Estos symlinks **siempre** apuntan al dispositivo correcto, sin importar el
orden de detección. Los veremos en detalle en T03 (udev).

### 8.3. Cuándo importa y cuándo no

```
Importa (nombres pueden cambiar):
  ✗ Servidores con múltiples discos SCSI/SATA
  ✗ Sistemas con USB drives que se conectan/desconectan
  ✗ Cualquier /etc/fstab que use /dev/sdX

No importa (nombres estables):
  ✓ VMs con discos virtio (vda siempre es vda)
  ✓ PCs con un solo disco
  ✓ NVMe (el controlador determina el nombre)
  ✓ Cuando usas UUID en fstab (buena práctica)
```

En nuestras VMs de lab, `vda` siempre será el disco del SO y `vdb`-`vde` los
discos extra, porque los adjuntamos en un orden determinista. Pero en producción,
**siempre usa UUID o identificadores persistentes**.

---

## 9. Errores comunes

### Error 1: confundir el disco con una partición

```bash
# ✗ Intentar montar el disco completo (sin partición ni filesystem)
sudo mount /dev/vdb /mnt
# Error: "wrong fs type, bad option, bad superblock on /dev/vdb"

# El disco /dev/vdb es el dispositivo completo — no tiene filesystem
# Primero necesitas crear una partición y un filesystem:
# sudo fdisk /dev/vdb   (crear partición vdb1)
# sudo mkfs.ext4 /dev/vdb1
# sudo mount /dev/vdb1 /mnt

# ✓ O crear filesystem directamente en el disco (sin partición):
sudo mkfs.ext4 /dev/vdb
sudo mount /dev/vdb /mnt
# Esto funciona pero no es recomendable en producción
```

### Error 2: asumir que sdX es estable

```bash
# ✗ En /etc/fstab:
/dev/sdb1  /data  ext4  defaults  0 2
# Si el orden de detección cambia, /dev/sdb1 podría ser otro disco

# ✓ Usar UUID:
UUID=a1b2c3d4-e5f6-7890-abcd-ef1234567890  /data  ext4  defaults  0 2
# El UUID es único e invariable
```

### Error 3: confundir NVMe particiones con namespaces

```bash
# ✗ "nvme0n1p1" — ¿es namespace 1 partición 1 o namespace 11?

# ✓ Desglose correcto:
# nvme0  = controlador 0
# n1     = namespace 1
# p1     = partición 1

# nvme0n1   = el disco completo
# nvme0n1p1 = la primera partición del disco
```

### Error 4: confundir vdX con sdX en una VM

```bash
# ✗ Buscar /dev/sda dentro de una VM con discos virtio
ls /dev/sda
# ls: cannot access '/dev/sda': No such file or directory

# ✓ Los discos virtio son /dev/vdX
ls /dev/vda
# /dev/vda

# Si ves /dev/sdX dentro de una VM, probablemente se creó con
# bus='ide' o bus='scsi' en vez de bus='virtio'
```

### Error 5: no distinguir disco de partición en lsblk

```bash
# La columna TYPE indica qué es cada cosa:
lsblk
# NAME   TYPE
# vda    disk    ← esto es un DISCO completo
# ├─vda1 part    ← esto es una PARTICIÓN
# vdb    disk    ← disco sin particionar (vacío)

# disk = dispositivo completo
# part = partición dentro de un disco
```

---

## 10. Cheatsheet

```bash
# ── Nomenclatura por bus ─────────────────────────────────────────
# /dev/sdX        SCSI, SATA, USB, SAS
# /dev/nvmeXnY    NVMe (SSD modernos)
# /dev/vdX        Virtio (VMs KVM/QEMU)
# /dev/hdX        IDE (legacy)
# /dev/loopN      Loop devices (archivo como disco)
# /dev/mdN        RAID software (mdadm)
# /dev/dm-N       Device mapper (LVM, LUKS)

# ── Particiones ──────────────────────────────────────────────────
# /dev/sda1       partición 1 de sda
# /dev/nvme0n1p1  partición 1 de nvme0n1 (nota la "p")
# /dev/vda1       partición 1 de vda

# ── MBR: numeración especial ────────────────────────────────────
# 1-4:  primarias (o 3 primarias + 1 extendida)
# 5+:   lógicas (dentro de la extendida)

# ── Identificadores persistentes ─────────────────────────────────
# /dev/disk/by-uuid/     por UUID del filesystem
# /dev/disk/by-id/       por modelo y serial del hardware
# /dev/disk/by-label/    por etiqueta del filesystem
# /dev/disk/by-path/     por ruta del bus PCI/USB
# /dev/disk/by-partuuid/ por UUID de la partición (GPT)

# ── Información rápida ──────────────────────────────────────────
ls -la /dev/sdX                # ver major:minor
lsblk                         # árbol de discos y particiones
ls /dev/disk/by-uuid/          # ver UUIDs
```

---

## 11. Ejercicios

### Ejercicio 1: identificar dispositivos en tu VM

1. Entra a tu VM de lab y ejecuta `lsblk`
2. Identifica:
   - ¿Cuántos discos hay? ¿Qué prefijo usan (sd, vd, nvme)?
   - ¿Cuáles tienen particiones y cuáles están vacíos?
   - ¿Qué partición está montada en `/`?
   - ¿Qué partición es swap?
3. Ejecuta `ls -la /dev/vda` — anota el major:minor number
4. Ejecuta `ls -la /dev/vdb` — ¿tiene el mismo major? ¿Qué minor tiene?

**Pregunta de reflexión**: los discos `vdb`-`vde` aparecen como `disk` en la
columna TYPE de `lsblk`, sin particiones. ¿Significa que no se pueden usar?
¿Cuál es la diferencia entre un disco y una partición desde el punto de vista
del kernel?

### Ejercicio 2: explorar /dev/disk/

1. Ejecuta `ls -la /dev/disk/by-uuid/` — ¿cuántas entradas ves?
2. Cada entrada es un symlink. ¿A qué dispositivo apunta cada una?
3. Ejecuta `ls -la /dev/disk/by-path/` — ¿ves los discos virtio?
4. Compara: ¿cuántas entradas hay en `by-uuid` vs en `by-path`?

**Pregunta de reflexión**: los discos `vdb`-`vde` no aparecen en `by-uuid`
porque no tienen filesystem. ¿Aparecerán después de crear un filesystem con
`mkfs.ext4`? ¿Y en `by-path`?

### Ejercicio 3: identificar dispositivos en tu host

1. En tu máquina host (no la VM), ejecuta `lsblk`
2. Identifica qué tipo de disco tienes: ¿sdX, nvmeXnY, o ambos?
3. Si tienes NVMe, identifica el controlador, namespace y particiones
4. Ejecuta `ls /dev/disk/by-id/` y busca el modelo y serial de tu disco

**Pregunta de reflexión**: si tu host tiene un SSD NVMe con el SO y un HDD
SATA para datos, ¿por qué el NVMe no aparece como `/dev/sda`? ¿Qué pasaría
si reemplazas el NVMe por un SSD SATA — cambiaría el nombre del dispositivo?
