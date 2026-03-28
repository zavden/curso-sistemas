# Tablas de Particiones: MBR vs GPT

## Índice

1. [Por qué existen las tablas de particiones](#1-por-qué-existen-las-tablas-de-particiones)
2. [MBR: Master Boot Record](#2-mbr-master-boot-record)
3. [GPT: GUID Partition Table](#3-gpt-guid-partition-table)
4. [MBR vs GPT: comparación directa](#4-mbr-vs-gpt-comparación-directa)
5. [Leer tablas de particiones: fdisk -l](#5-leer-tablas-de-particiones-fdisk--l)
6. [Crear particiones con fdisk](#6-crear-particiones-con-fdisk)
7. [gdisk: fdisk para GPT](#7-gdisk-fdisk-para-gpt)
8. [parted: la herramienta universal](#8-parted-la-herramienta-universal)
9. [partprobe: notificar al kernel](#9-partprobe-notificar-al-kernel)
10. [Particiones en contexto QEMU/KVM](#10-particiones-en-contexto-qemukvm)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Por qué existen las tablas de particiones

Un disco es una secuencia lineal de sectores (bloques de 512 bytes o 4096 bytes). Sin estructura, es un bloque amorfo de bytes. La **tabla de particiones** es una estructura de datos al inicio del disco que dice: "los sectores 2048–1048575 son la partición 1, los sectores 1048576–2097151 son la partición 2..."

```
 Disco sin tabla de particiones (virgen)
┌──────────────────────────────────────────────────┐
│ 0000000000000000000000000000000000000000000000000 │
│ (20 GiB de nada — no se puede montar ni usar)    │
└──────────────────────────────────────────────────┘

 Disco con tabla de particiones GPT
┌─────┬────────────┬──────────────────┬────────────┐
│ GPT │ Partición 1│   Partición 2    │ Partición 3│
│hdr  │  /boot     │   /              │  /data     │
│     │  1 GiB     │   15 GiB         │  4 GiB     │
└─────┴────────────┴──────────────────┴────────────┘
  ↑
  La tabla solo dice dónde empieza y termina cada partición
```

¿Por qué dividir un disco en particiones en vez de usar uno solo?

- **Aislamiento**: si `/home` se llena, `/` sigue funcionando.
- **Diferentes filesystems**: `/boot` puede ser ext4 mientras `/` es xfs.
- **Diferentes opciones de montaje**: `/tmp` puede ser `noexec,nosuid` mientras `/home` no.
- **Backup selectivo**: puedes hacer snapshot de una partición sin tocar las demás.
- **Requisitos de boot**: UEFI necesita una partición ESP (EFI System Partition) separada.

---

## 2. MBR: Master Boot Record

MBR es el esquema de particiones original de los PCs IBM de 1983. Sigue en uso en discos pequeños y sistemas legacy, pero está siendo reemplazado por GPT.

### Estructura del sector 0

El MBR ocupa exactamente **un sector** (512 bytes) al inicio del disco — el sector LBA 0:

```
 Byte 0                                      Byte 511
┌──────────────────┬──────────────────────────┬────┐
│   Boot code      │  Tabla de particiones    │Sig │
│   (446 bytes)    │  (4 entradas × 16 bytes) │    │
│                  │  = 64 bytes              │    │
│  Código del      ├──────┬──────┬──────┬─────┤0x55│
│  bootloader      │  P1  │  P2  │  P3  │ P4  │0xAA│
│  (GRUB stage 1)  │16 B  │16 B  │16 B  │16 B │    │
└──────────────────┴──────┴──────┴──────┴─────┴────┘
```

| Parte | Tamaño | Contenido |
|-------|:------:|-----------|
| Boot code | 446 bytes | Código del bootloader (GRUB stage 1, etc.) |
| Partition table | 64 bytes | 4 entradas de 16 bytes cada una |
| Signature | 2 bytes | `0x55AA` — marca el sector como MBR válido |

### Cada entrada de partición (16 bytes)

```
┌──────┬──────────┬──────┬──────────┬──────────┬──────────┐
│Status│CHS start │Type  │CHS end   │LBA start │Sectors   │
│1 byte│3 bytes   │1 byte│3 bytes   │4 bytes   │4 bytes   │
└──────┴──────────┴──────┴──────────┴──────────┴──────────┘
```

- **Status**: `0x80` = partición activa (booteable), `0x00` = inactiva.
- **Type**: código hexadecimal que indica el tipo — `0x83` = Linux, `0x82` = Linux swap, `0x07` = NTFS, `0x0C` = FAT32 LBA.
- **LBA start**: sector donde empieza la partición (4 bytes = 32 bits).
- **Sectors**: número total de sectores (4 bytes = 32 bits).

### Limitaciones de MBR

El campo de LBA start y Sectors tiene 32 bits cada uno. Con sectores de 512 bytes:

```
 2^32 sectores × 512 bytes/sector = 2 TiB máximo direccionable
```

| Limitación | Valor |
|------------|-------|
| Máximo tamaño de disco | 2 TiB |
| Máximo particiones primarias | 4 |
| Máximo particiones totales | 4 primarias, o 3 primarias + 1 extendida (con hasta 128 lógicas) |
| Redundancia | Ninguna — si se corrompe el sector 0, se pierde toda la tabla |

### Particiones primarias, extendidas y lógicas

Como MBR solo tiene espacio para 4 entradas, se inventó un truco para superar este límite:

```
 Disco con MBR
┌─────┬──────────┬──────────┬──────────────────────────┐
│ MBR │ Primaria │ Primaria │      Extendida           │
│     │    1     │    2     │ ┌────────┬──────────────┐ │
│     │ /boot    │ swap     │ │Lógica 5│  Lógica 6    │ │
│     │ sda1     │ sda2     │ │  /     │  /home       │ │
│     │          │          │ │ sda5   │  sda6        │ │
│     │          │          │ └────────┴──────────────┘ │
└─────┴──────────┴──────────┴──────────────────────────┘
  ↑ 4 entradas     P1  P2       P3 (extendida)
    en la tabla                 contiene lógicas
```

- **Primaria**: partición real, máximo 4. Numeradas `sda1`–`sda4`.
- **Extendida**: un contenedor que ocupa una de las 4 entradas primarias. No contiene datos directamente.
- **Lógica**: particiones dentro de la extendida. Numeradas desde `sda5` en adelante.

La partición extendida usa una cadena de **EBR** (Extended Boot Records) — cada lógica tiene un mini-MBR que apunta a la siguiente, formando una lista enlazada.

En la práctica, GPT elimina toda esta complejidad.

---

## 3. GPT: GUID Partition Table

GPT (GUID Partition Table) es parte del estándar UEFI y reemplaza a MBR. Todos los sistemas modernos usan GPT.

### Estructura de GPT

GPT usa múltiples sectores al inicio Y al final del disco:

```
 LBA 0           LBA 1          LBA 2–33        ...        LBA -33 a -1
┌──────────┬───────────┬─────────────────┬─────────┬─────────────────┐
│Protective│ GPT       │ Partition       │  ...    │ Backup GPT      │
│   MBR    │ Header    │ Entries         │ (datos) │ Header+Entries  │
│(compat.) │           │ (128 entradas   │         │ (copia exacta)  │
│          │           │  × 128 bytes)   │         │                 │
└──────────┴───────────┴─────────────────┴─────────┴─────────────────┘
```

| Componente | LBA | Tamaño | Función |
|-----------|:---:|--------|---------|
| Protective MBR | 0 | 1 sector | Evita que herramientas MBR vean el disco como vacío |
| GPT Header | 1 | 1 sector | Metadatos: GUID del disco, número de entradas, CRC32 |
| Partition Entries | 2–33 | 32 sectores | 128 entradas × 128 bytes cada una |
| Backup Entries | -33 a -2 | 32 sectores | Copia de las entradas (recuperación) |
| Backup Header | -1 | 1 sector | Copia del header (recuperación) |

### Cada entrada GPT (128 bytes)

```
┌──────────────┬──────────────┬──────────────┬──────────┬──────────────┐
│Partition Type│Unique GUID   │First LBA     │Last LBA  │  Name        │
│  GUID        │(PARTUUID)    │(8 bytes)     │(8 bytes) │  (72 bytes   │
│ (16 bytes)   │(16 bytes)    │              │          │   UTF-16)    │
└──────────────┴──────────────┴──────────────┴──────────┴──────────────┘
```

Cada partición tiene un **GUID único** (el PARTUUID que ves con `blkid`) y un nombre legible de hasta 36 caracteres UTF-16.

### Ventajas de GPT sobre MBR

| Característica | MBR | GPT |
|---------------|:---:|:---:|
| Tamaño máximo de disco | 2 TiB | 8 ZiB (teórico) |
| Máximo de particiones | 4 primarias (o 3+extendida) | 128 (por defecto) |
| Redundancia | Ninguna | Header + entries duplicados al final |
| Integridad | Sin verificación | CRC32 en header y entries |
| Identificación | Número de tipo (1 byte) | GUID de tipo (16 bytes) |
| Nombre de partición | No soportado | Hasta 36 caracteres UTF-16 |
| Direccionamiento | 32-bit LBA | 64-bit LBA |
| Boot firmware | BIOS | UEFI (y BIOS con Protective MBR) |

### Protective MBR

GPT mantiene un MBR "falso" en LBA 0 con una sola partición de tipo `0xEE` que cubre todo el disco. Esto protege contra herramientas MBR antiguas que podrían ver el disco como vacío y sobrescribirlo. Si ves una partición tipo `0xEE`, significa "este disco usa GPT, no toques el MBR".

---

## 4. MBR vs GPT: comparación directa

### Cuándo usar cada uno

```
 ¿Qué tabla de particiones usar?

 ┌──────────────────────────┐
 │ ¿El disco es > 2 TiB?   │
 │          │               │
 │    Sí    │    No         │
 │    │     │    │          │
 │    ▼     │    ▼          │
 │   GPT    │ ¿UEFI?       │
 │(obligat.)│    │          │
 │          │ Sí │  No      │
 │          │  │ │   │      │
 │          │  ▼ │   ▼      │
 │          │ GPT│  MBR o   │
 │          │    │  GPT*    │
 └──────────┴────┴──────────┘

 * En VMs con BIOS emulado, MBR funciona. GPT también
   funciona con BIOS+GRUB (necesita partición BIOS boot).
```

**En el contexto del curso**: las VMs de QEMU/KVM suelen usar firmware BIOS emulado (SeaBIOS) por defecto. El instalador de Debian/Fedora elige MBR o GPT automáticamente según el firmware. Para discos de datos adicionales en VMs, GPT es la opción recomendada por su simplicidad (sin complicaciones de primaria/extendida/lógica).

### Identificar qué tabla tiene un disco

```bash
# Con fdisk
sudo fdisk -l /dev/vda
# Disklabel type: gpt     ← o "dos" para MBR

# Con blkid
sudo blkid -p /dev/vda
# PTTYPE="gpt"            ← o "dos" para MBR

# Con gdisk (para GPT)
sudo gdisk -l /dev/vda
# Partition table scan:
#   MBR: protective
#   GPT: present           ← confirma GPT

# Con parted
sudo parted /dev/vda print
# Partition Table: gpt     ← o "msdos" para MBR
```

---

## 5. Leer tablas de particiones: fdisk -l

`fdisk -l` es la forma más directa de leer la tabla de particiones de un disco. No modifica nada — solo muestra.

### Disco GPT

```bash
sudo fdisk -l /dev/vda
```

```
Disk /dev/vda: 20 GiB, 21474836480 bytes, 41943040 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / 512 bytes
Disklabel type: gpt
Disk identifier: 3F2504E0-4F89-11D3-9A0C-0305E82C3301

Device      Start      End  Sectors  Size Type
/dev/vda1    2048     4095     2048    1M BIOS boot
/dev/vda2    4096  2101247  2097152    1G EFI System
/dev/vda3 2101248 41943006 39841759   19G Linux filesystem
```

Desglose de la cabecera:

| Línea | Significado |
|-------|-------------|
| `Disk /dev/vda: 20 GiB` | Tamaño total del disco |
| `41943040 sectors` | Total de sectores |
| `Sector size: 512 bytes` | Tamaño de cada sector |
| `Disklabel type: gpt` | Tipo de tabla de particiones |
| `Disk identifier: 3F25...` | GUID del disco (no confundir con PARTUUID de particiones) |

Desglose de cada partición:

| Columna | Significado |
|---------|-------------|
| **Device** | Ruta del dispositivo de la partición |
| **Start** | Sector inicial (nota: 2048, no 0 — los primeros sectores son la tabla GPT) |
| **End** | Último sector de la partición |
| **Sectors** | Total de sectores |
| **Size** | Tamaño legible |
| **Type** | Tipo de partición según el GUID de tipo |

### Disco MBR

```bash
sudo fdisk -l /dev/sda
```

```
Disk /dev/sda: 20 GiB, 21474836480 bytes, 41943040 sectors
Disklabel type: dos
Disk identifier: 0x0003a7e4

Device     Boot   Start      End  Sectors  Size Id  Type
/dev/sda1  *       2048  2099199  2097152    1G 83  Linux
/dev/sda2       2099200  4196351  2097152    1G 82  Linux swap
/dev/sda3       4196352 41943039 37746688   18G 83  Linux
```

Diferencias con GPT:
- `Disklabel type: dos` (MBR se llama "dos" en fdisk).
- La columna **Boot** muestra `*` en la partición marcada como activa.
- La columna **Id** muestra el código hexadecimal de tipo (83 = Linux, 82 = swap).

### Listar todos los discos

```bash
sudo fdisk -l
# Muestra la tabla de particiones de TODOS los discos detectados
```

---

## 6. Crear particiones con fdisk

`fdisk` es interactivo: abres el disco, ejecutas comandos con letras, y al final escribes los cambios. **Nada se modifica hasta que presionas `w`** — puedes cancelar con `q` en cualquier momento.

### Flujo en un disco virgen

Supongamos un disco `/dev/vdb` recién añadido a la VM, sin tabla de particiones:

```bash
sudo fdisk /dev/vdb
```

```
Welcome to fdisk (util-linux 2.39).

Device does not contain a recognized partition table.
Created a new DOS disklabel with disk identifier 0x12345678.

Command (m for help):
```

Nota: fdisk creó una tabla MBR (`DOS disklabel`) por defecto. Para GPT, usa `g` primero.

### Comandos principales de fdisk

| Comando | Acción |
|:-------:|--------|
| `m` | Mostrar ayuda |
| `p` | Imprimir tabla actual (print) |
| `g` | Crear nueva tabla GPT (borra MBR si existía) |
| `o` | Crear nueva tabla MBR (borra GPT si existía) |
| `n` | Crear nueva partición |
| `d` | Borrar una partición |
| `t` | Cambiar el tipo de una partición |
| `l` | Listar tipos de partición disponibles |
| `w` | **Escribir cambios y salir** (¡irreversible!) |
| `q` | Salir sin guardar |

### Ejemplo completo: crear dos particiones GPT

```bash
sudo fdisk /dev/vdb
```

**Paso 1: crear tabla GPT**

```
Command: g
Created a new GPT disklabel (GUID: ...).
```

**Paso 2: crear partición 1 (2 GiB)**

```
Command: n
Partition number (1-128, default 1): ↵        # Enter = 1
First sector (2048-10485726, default 2048): ↵  # Enter = 2048
Last sector ... (default 10485726): +2G        # +tamaño

Created a new partition 1 of type 'Linux filesystem' and of size 2 GiB.
```

El formato `+<tamaño>` acepta:
- `+2G` → 2 GiB
- `+500M` → 500 MiB
- `+1024K` → 1024 KiB
- Sin `+` → sector exacto

**Paso 3: crear partición 2 (el resto)**

```
Command: n
Partition number (2-128, default 2): ↵
First sector (4196352-10485726, default 4196352): ↵  # Empieza donde terminó la anterior
Last sector ... (default 10485726): ↵                # Enter = usar todo el espacio restante

Created a new partition 2 of type 'Linux filesystem' and of size 3 GiB.
```

**Paso 4: verificar antes de escribir**

```
Command: p

Device      Start      End  Sectors Size Type
/dev/vdb1    2048  4196351  4194304   2G Linux filesystem
/dev/vdb2 4196352 10485726  6289375   3G Linux filesystem
```

**Paso 5: escribir y salir**

```
Command: w
The partition table has been altered.
Calling ioctl() to re-read partition table.
Syncing disks.
```

Ahora `/dev/vdb1` y `/dev/vdb2` existen. Puedes verificar con `lsblk`:

```bash
lsblk /dev/vdb
# vdb    252:16   0    5G  0 disk
# ├─vdb1 252:17   0    2G  0 part
# └─vdb2 252:18   0    3G  0 part
```

### Cambiar el tipo de partición

Por defecto, fdisk asigna tipo "Linux filesystem" a todas las particiones. Para swap u otros usos:

```bash
Command: t
Partition number (1,2, default 2): 2
Partition type or alias (type L to list all): 19   # 19 = Linux swap

Changed type of partition 'Linux filesystem' to 'Linux swap'.
```

Tipos GPT comunes:

| Código fdisk | Tipo | Uso |
|:------------:|------|-----|
| 1 | EFI System | Partición ESP para UEFI boot |
| 4 | BIOS boot | Partición para GRUB en GPT+BIOS |
| 19 | Linux swap | Área de swap |
| 20 | Linux filesystem | Particiones Linux normales (por defecto) |
| 30 | Linux LVM | Volúmenes físicos para LVM |

### Borrar particiones

```
Command: d
Partition number (1,2): 2
Partition 2 has been deleted.
```

Recuerda: nada se escribe hasta `w`.

---

## 7. gdisk: fdisk para GPT

`gdisk` (GPT fdisk) es la herramienta nativa para tablas GPT. Su interfaz es casi idéntica a fdisk:

```bash
sudo gdisk /dev/vdb
```

```
GPT fdisk (gdisk) version 1.0.9
Partition table scan:
  MBR: protective
  GPT: present

Found valid GPT with protective MBR; using GPT.

Command (? for help):
```

### Diferencias con fdisk

| Acción | fdisk | gdisk |
|--------|-------|-------|
| Crear tabla GPT | `g` | Automático (es su formato nativo) |
| Crear tabla MBR | `o` | No soportado |
| Ayuda | `m` | `?` |
| Verificar integridad | No | `v` (verify) |
| Recuperar backup GPT | No | `b` (backup), `c` (load backup) |
| Modo experto | No | `x` |

### Cuándo usar gdisk en vez de fdisk

- Necesitas funciones avanzadas de GPT (recovery, backup, verificación de CRC).
- Quieres convertir MBR a GPT sin destruir datos (gdisk puede intentarlo).
- Trabajas exclusivamente con GPT.

En la mayoría de casos prácticos con VMs, `fdisk` moderno (util-linux ≥ 2.26) maneja GPT perfectamente y es más familiar.

---

## 8. parted: la herramienta universal

`parted` maneja tanto MBR como GPT y tiene una diferencia crítica: **los cambios se aplican inmediatamente**, no hay "escribir y salir". Cada comando modifica el disco en el acto.

### Modo interactivo

```bash
sudo parted /dev/vdb
(parted) print

Model: Virtio Block Device (virtblk)
Disk /dev/vdb: 5369MB
Sector size (logical/physical): 512B/512B
Partition Table: gpt
Disk Flags:

Number  Start   End     Size    File system  Name  Flags
 1      1049kB  2149MB  2147MB
 2      2149MB  5369MB  3221MB
```

### Comandos parted vs fdisk

| Acción | parted | fdisk |
|--------|--------|-------|
| Crear tabla GPT | `mklabel gpt` | `g` |
| Crear tabla MBR | `mklabel msdos` | `o` |
| Crear partición | `mkpart primary ext4 0% 50%` | `n` interactivo |
| Borrar partición | `rm 1` | `d` |
| Ver tabla | `print` | `p` |
| Activar flag | `set 1 boot on` | `a` |
| Redimensionar | `resizepart 1 3G` | No soportado |

### Modo no-interactivo (una línea)

```bash
# Crear tabla GPT
sudo parted -s /dev/vdb mklabel gpt

# Crear partición que use todo el disco
sudo parted -s /dev/vdb mkpart primary ext4 0% 100%

# Crear dos particiones (50% cada una)
sudo parted -s /dev/vdb mkpart primary ext4 0% 50%
sudo parted -s /dev/vdb mkpart primary xfs 50% 100%
```

El flag `-s` (script mode) elimina las preguntas interactivas. Útil para automatización, pero peligroso — no hay confirmación.

### La diferencia crucial con fdisk

```
 fdisk/gdisk: los cambios se acumulan en memoria → escribes con 'w' o cancelas con 'q'
              ¡Puedes experimentar sin miedo!

 parted:      cada comando se ejecuta INMEDIATAMENTE en el disco
              No hay 'undo'. Si borras una partición, desapareció.
```

Para practicar y aprender, fdisk es más seguro porque puedes cancelar. Para scripts y automatización, parted es más conveniente por su modo no-interactivo.

---

## 9. partprobe: notificar al kernel

Cuando modificas la tabla de particiones, el kernel puede no enterarse automáticamente — especialmente si el disco está en uso o es un loop device. `partprobe` fuerza una relectura:

```bash
# Re-leer la tabla de particiones de un disco
sudo partprobe /dev/vdb

# Re-leer TODOS los discos
sudo partprobe
```

### Cuándo es necesario

| Situación | ¿Partprobe necesario? |
|-----------|:---------------------:|
| `fdisk` en disco no montado | Generalmente no (fdisk lo hace) |
| `fdisk` en disco con particiones montadas | Sí |
| Loop device tras crear particiones | Sí |
| `parted` en cualquier caso | Generalmente no (parted notifica) |
| Disco añadido en caliente a VM | No (el kernel detecta el nuevo disco) |

Si después de particionar no ves las particiones nuevas con `lsblk`:

```bash
# Paso 1: intentar partprobe
sudo partprobe /dev/vdb

# Paso 2: si falla, verificar si algo está montado
mount | grep vdb

# Paso 3: como último recurso (en VMs de test, no en producción)
# reiniciar
```

### kpartx: para imágenes de disco

Si particionaste un loop device o una imagen raw, `kpartx` crea los device maps de las particiones:

```bash
sudo kpartx -av /dev/loop0
# add map loop0p1 (253:0): 0 4194304 linear 7:0 2048
# add map loop0p2 (253:1): 0 6289375 linear 7:0 4196352

# Las particiones aparecen en /dev/mapper/
ls /dev/mapper/loop0p*
# /dev/mapper/loop0p1  /dev/mapper/loop0p2

# Limpiar cuando termines
sudo kpartx -dv /dev/loop0
```

---

## 10. Particiones en contexto QEMU/KVM

### Discos virtuales nuevos: vacíos

Cuando creas un disco con `qemu-img create`, obtienes un disco completamente vacío — sin tabla de particiones, sin filesystem, sin nada:

```bash
# En el host
qemu-img create -f qcow2 /var/lib/libvirt/images/data-disk.qcow2 5G
virsh attach-disk debian-lab /var/lib/libvirt/images/data-disk.qcow2 vdb \
  --driver qemu --subdriver qcow2 --persistent

# Dentro de la VM
lsblk
# vdb  252:16  0  5G  0 disk    ← sin particiones, sin nada

sudo fdisk -l /dev/vdb
# Disk /dev/vdb: 5 GiB, 5368709120 bytes, 10485760 sectors
# (sin tabla de particiones)
```

El flujo completo para usar este disco:

```
 qemu-img create    virsh attach-disk    fdisk /dev/vdb     mkfs.ext4      mount
    (host)              (host)            (VM)             /dev/vdb1       /data
      │                   │                 │                 │              │
      ▼                   ▼                 ▼                 ▼              ▼
  Crear imagen →   Conectar a VM →    Particionar →    Crear FS →    Montar y usar
  (archivo)        (aparece vdb)      (crear vdb1)     (ext4/xfs)    (añadir a fstab)
```

### El layout típico de una VM Linux

Cuando instalas Debian o Fedora en una VM, el instalador crea particiones automáticamente:

```bash
# VM Debian instalada con defaults
sudo fdisk -l /dev/vda

Device      Start      End  Sectors  Size Type
/dev/vda1    2048     4095     2048    1M BIOS boot     ← para GRUB en BIOS+GPT
/dev/vda2    4096  2101247  2097152    1G Linux filesystem  ← /boot
/dev/vda3 2101248 41943006 39841759   19G Linux filesystem  ← / (root)
```

- **vda1 (1M, BIOS boot)**: partición diminuta que GRUB necesita cuando el firmware es BIOS y la tabla es GPT. No tiene filesystem — GRUB escribe su stage 2 aquí directamente.
- **vda2 (1G, /boot)**: kernel, initramfs, configuración de GRUB.
- **vda3 (resto, /)**: el sistema completo.

En VMs con UEFI, verías una partición **EFI System** (FAT32) en vez de BIOS boot.

### Disco de datos adicional: ¿particionar o usar el disco entero?

Para discos de datos simples en VMs, hay dos enfoques:

**Opción A: crear partición + filesystem** (recomendado)

```bash
sudo fdisk /dev/vdb
# g → n → w (una partición GPT que usa todo el disco)
sudo mkfs.ext4 /dev/vdb1
sudo mount /dev/vdb1 /data
```

**Opción B: filesystem directamente en el disco** (sin particionar)

```bash
sudo mkfs.ext4 /dev/vdb    # sin número de partición
sudo mount /dev/vdb /data
```

La opción B funciona, pero tiene desventajas:
- No puedes dividir el disco después sin destruir el filesystem.
- `blkid` y `fdisk` muestran el disco sin tabla de particiones — puede confundir.
- Algunos instaladores y herramientas asumen que los discos tienen tabla de particiones.

**Recomendación**: siempre crear al menos una partición GPT, incluso si solo necesitas una.

---

## 11. Errores comunes

### Error 1: olvidar que fdisk necesita `w` para guardar

```bash
sudo fdisk /dev/vdb
# n → crear partición → p → se ve bien → q
# "¿Por qué no aparece la partición?"
```

**Por qué falla**: `q` sale SIN guardar. Los cambios solo existían en memoria.

**Solución**: usar `w` para escribir los cambios al disco. Si no estás seguro, usa `p` para revisar antes de `w`.

### Error 2: crear MBR cuando necesitas GPT (o viceversa)

```bash
sudo fdisk /dev/vdb
# Empieza a crear particiones... pero el disco quedó con MBR
# Ahora quieres GPT pero ya tienes datos
```

**Por qué pasa**: fdisk crea MBR por defecto. Si no ejecutas `g` primero, obtienes tabla MBR.

**Solución**: siempre ejecutar `g` (GPT) o `o` (MBR) explícitamente como primer paso en un disco nuevo. Si ya creaste particiones con la tabla equivocada:

```bash
# ⚠️ Esto DESTRUYE todas las particiones
sudo fdisk /dev/vdb
# g   ← crear tabla GPT nueva (borra la MBR y todas sus particiones)
# n   ← recrear particiones
# w   ← guardar
```

### Error 3: no entender la numeración con MBR

```bash
# Creaste 3 primarias + 1 extendida con 2 lógicas en MBR
lsblk /dev/sda
# sda1  ← primaria
# sda2  ← primaria
# sda3  ← primaria
# sda4  ← extendida (¡no es usable directamente!)
# sda5  ← lógica (primera dentro de la extendida)
# sda6  ← lógica (segunda)

# "¿Por qué sda4 no se puede montar? ¿Y por qué saltó a sda5?"
```

**Por qué pasa**: en MBR, `sda4` (si es extendida) es un contenedor, no una partición usable. Las lógicas empiezan desde `sda5` siempre, sin importar cuántas primarias hay.

**Solución**: usar GPT. Si debes usar MBR, recuerda: la extendida no se monta; las lógicas empiezan en 5.

### Error 4: usar parted pensando que funciona como fdisk

```bash
sudo parted /dev/vdb
(parted) rm 1
# "Solo quería ver qué pasaría"
# La partición ya se borró. No hay undo.
```

**Por qué pasa**: parted ejecuta cambios inmediatamente, a diferencia de fdisk que los acumula en memoria.

**Solución**: para experimentar, usar fdisk (cancelar con `q`). Para automatización, usar `parted -s`. Nunca "probar" comandos destructivos en parted interactivo con discos que tengan datos.

### Error 5: particionar un disco que ya tiene filesystem sin tabla

```bash
sudo fdisk -l /dev/vdb
# "No tiene tabla de particiones"
# Pero...
sudo blkid /dev/vdb
# /dev/vdb: UUID="..." TYPE="ext4"
# ¡Alguien hizo mkfs directamente en el disco sin particionar!

sudo fdisk /dev/vdb
# g → n → w
# Acabas de destruir el filesystem que estaba ahí
```

**Por qué pasa**: si alguien creó un filesystem directamente en `/dev/vdb` (sin particionar), el disco tiene datos útiles pero no tabla de particiones. Crear una tabla destruye el superbloque del filesystem.

**Solución**: verificar siempre con `blkid` antes de particionar un disco "vacío":

```bash
sudo blkid /dev/vdb
# Si muestra TYPE=... → ¡tiene un filesystem, no está vacío!
# Si no muestra nada → genuinamente vacío, seguro particionar
```

---

## 12. Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│           TABLAS DE PARTICIONES — REFERENCIA                     │
├──────────────────────────────────────────────────────────────────┤
│ IDENTIFICAR TABLA                                                │
│   fdisk -l /dev/vdb           "Disklabel type: gpt" o "dos"    │
│   blkid -p /dev/vdb           "PTTYPE=gpt" o "dos"             │
│   parted /dev/vdb print       "Partition Table: gpt" o "msdos" │
├──────────────────────────────────────────────────────────────────┤
│ fdisk (INTERACTIVO — cambios se guardan con 'w')                │
│   g                           Crear tabla GPT                   │
│   o                           Crear tabla MBR                   │
│   n                           Nueva partición                   │
│   d                           Borrar partición                  │
│   t                           Cambiar tipo de partición         │
│   p                           Imprimir tabla actual             │
│   w                           ESCRIBIR y salir                  │
│   q                           Salir SIN guardar                 │
├──────────────────────────────────────────────────────────────────┤
│ gdisk (INTERACTIVO — solo GPT)                                   │
│   n/d/p/w/q                   Igual que fdisk                   │
│   v                           Verificar integridad GPT          │
│   ?                           Ayuda                             │
├──────────────────────────────────────────────────────────────────┤
│ parted (¡CAMBIOS INMEDIATOS!)                                    │
│   mklabel gpt                 Crear tabla GPT                   │
│   mklabel msdos               Crear tabla MBR                   │
│   mkpart primary ext4 0% 50%  Crear partición                   │
│   rm 1                        Borrar partición 1                │
│   resizepart 1 3G             Redimensionar partición           │
│   print                       Mostrar tabla                     │
│   -s (flag)                   Modo script (sin preguntas)       │
├──────────────────────────────────────────────────────────────────┤
│ AUXILIARES                                                       │
│   partprobe /dev/vdb          Re-leer tabla de particiones      │
│   kpartx -av /dev/loop0       Mapear particiones de loop device │
│   kpartx -dv /dev/loop0       Eliminar mapeos                   │
├──────────────────────────────────────────────────────────────────┤
│ FLUJO TÍPICO EN VM                                               │
│   1. qemu-img create (host)   Crear imagen de disco             │
│   2. virsh attach-disk (host) Conectar a la VM                  │
│   3. fdisk /dev/vdb (VM)      g → n → w                        │
│   4. mkfs.ext4 /dev/vdb1 (VM) Crear filesystem (→ S02)         │
│   5. mount /dev/vdb1 /data    Montar (→ S02)                   │
└──────────────────────────────────────────────────────────────────┘
```

---

## 13. Ejercicios

### Ejercicio 1: Investigar las particiones de tu sistema

1. Ejecuta `sudo fdisk -l` en tu host (o VM principal). Identifica:
   - ¿Es MBR o GPT?
   - ¿Cuántas particiones hay?
   - ¿Hay una partición BIOS boot o EFI System?
   - ¿Cuál es el sector de inicio de la primera partición? ¿Por qué no empieza en el sector 0?
2. Ejecuta `sudo parted /dev/vda print` (o `/dev/sda`) y compara la salida con fdisk. ¿Qué información muestra parted que fdisk no, y viceversa?
3. Si tienes una VM corriendo, ejecuta `sudo fdisk -l` dentro de ella y compara con el host.

> **Pregunta de reflexión**: la primera partición empieza en el sector 2048 (1 MiB de offset). ¿Por qué se desperdicia ese espacio al inicio? Investiga "partition alignment" y su relación con el rendimiento de SSDs y discos con sectores de 4096 bytes.

### Ejercicio 2: Particionar un loop device con fdisk

1. Crea un archivo de 1 GiB y asócialo a un loop device:
   ```bash
   dd if=/dev/zero of=/tmp/disk-practice.img bs=1M count=1024
   sudo losetup -f /tmp/disk-practice.img
   ```
2. Abre el loop device con fdisk. Crea una tabla GPT (`g`).
3. Crea tres particiones:
   - Partición 1: 200 MiB, tipo "Linux filesystem"
   - Partición 2: 300 MiB, tipo "Linux swap" (usa `t` → `19`)
   - Partición 3: el resto, tipo "Linux filesystem"
4. Antes de escribir, usa `p` para revisar. ¿Los tamaños coinciden con lo que pediste?
5. Escribe con `w`.
6. Ejecuta `sudo partprobe /dev/loop0` y verifica con `lsblk` que aparecen las 3 particiones.
7. Limpia: `sudo losetup -d /dev/loop0 && rm /tmp/disk-practice.img`

> **Pregunta de reflexión**: si hubieras creado una tabla MBR en vez de GPT y necesitaras más de 4 particiones, ¿cómo lo resolverías? ¿Cuántas particiones lógicas caben dentro de una extendida?

### Ejercicio 3: Disco adicional en una VM

Si tienes una VM con QEMU/KVM:

1. En el host, crea una imagen de 2 GiB:
   ```bash
   qemu-img create -f qcow2 /var/lib/libvirt/images/practice-disk.qcow2 2G
   ```
2. Adjúntala a tu VM:
   ```bash
   virsh attach-disk <tu-vm> /var/lib/libvirt/images/practice-disk.qcow2 vdb \
     --driver qemu --subdriver qcow2 --persistent
   ```
3. Dentro de la VM, verifica que `/dev/vdb` aparece con `lsblk`. Confirma que está vacío con `sudo blkid /dev/vdb`.
4. Usa fdisk para crear una tabla GPT con dos particiones: una de 1 GiB y otra con el resto.
5. Verifica con `lsblk` y `sudo fdisk -l /dev/vdb`.
6. **No formatees todavía** — eso se hace en S02 T01 (Sistemas de Archivos). Deja las particiones creadas para el siguiente tópico.

> **Pregunta de reflexión**: dentro de la VM, `/dev/vdb` es un disco de 2 GiB. En el host, el archivo qcow2 probablemente ocupa mucho menos que 2 GiB. ¿Crear la tabla de particiones con fdisk hizo que el archivo creciera en el host? Verifica con `qemu-img info` y `ls -lh`.
