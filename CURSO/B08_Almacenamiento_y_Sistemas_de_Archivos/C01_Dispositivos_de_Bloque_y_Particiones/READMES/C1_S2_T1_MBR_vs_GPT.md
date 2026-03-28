# MBR vs GPT

## Índice

1. [Qué es una tabla de particiones](#1-qué-es-una-tabla-de-particiones)
2. [MBR: Master Boot Record](#2-mbr-master-boot-record)
3. [GPT: GUID Partition Table](#3-gpt-guid-partition-table)
4. [Comparación directa](#4-comparación-directa)
5. [MBR y BIOS vs GPT y UEFI](#5-mbr-y-bios-vs-gpt-y-uefi)
6. [Cuándo usar cada uno](#6-cuándo-usar-cada-uno)
7. [Identificar el tipo en un disco](#7-identificar-el-tipo-en-un-disco)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. Qué es una tabla de particiones

Una tabla de particiones es una estructura de datos almacenada al inicio del
disco que describe cómo está dividido en particiones. Sin ella, el sistema
operativo no sabe dónde empieza ni dónde termina cada partición.

```
┌─────────────────────────────────────────────────────────┐
│                     DISCO FÍSICO                        │
│                                                         │
│  ┌──────────┬──────────┬──────────┬──────────────────┐  │
│  │  Tabla   │ Partición│ Partición│    Partición     │  │
│  │   de     │    1     │    2     │       3          │  │
│  │particion.│  (ext4)  │  (swap)  │    (xfs)         │  │
│  │          │          │          │                  │  │
│  └──────────┴──────────┴──────────┴──────────────────┘  │
│  ↑                                                      │
│  Sector 0 (primer sector del disco)                     │
└─────────────────────────────────────────────────────────┘
```

Existen dos formatos de tabla de particiones en uso:

- **MBR** (Master Boot Record) — el formato clásico, desde 1983
- **GPT** (GUID Partition Table) — el formato moderno, parte del estándar UEFI

---

## 2. MBR: Master Boot Record

### 2.1. Estructura

El MBR ocupa exactamente el **primer sector** del disco (512 bytes). Esos 512
bytes contienen todo:

```
┌──────────────────────────────────────────────────┐
│           MBR — Sector 0 (512 bytes)             │
│                                                  │
│  Bytes 0-445:    Bootstrap code (bootloader)     │
│                  Código que el BIOS ejecuta       │
│                  al arrancar                      │
│                                                  │
│  Bytes 446-509:  Partition table (64 bytes)       │
│                  4 entradas × 16 bytes cada una   │
│                  ┌────────────────────────┐       │
│                  │ Entrada 1 (16 bytes)   │       │
│                  │ Entrada 2 (16 bytes)   │       │
│                  │ Entrada 3 (16 bytes)   │       │
│                  │ Entrada 4 (16 bytes)   │       │
│                  └────────────────────────┘       │
│                                                  │
│  Bytes 510-511:  Boot signature (0x55AA)          │
│                  Marca que indica MBR válido      │
└──────────────────────────────────────────────────┘
```

### 2.2. Cada entrada de la tabla

Cada una de las 4 entradas (16 bytes) describe una partición:

| Offset | Tamaño | Contenido |
|--------|--------|-----------|
| 0 | 1 byte | Status (0x80 = bootable, 0x00 = no) |
| 1-3 | 3 bytes | CHS del primer sector |
| 4 | 1 byte | Tipo de partición (0x83 = Linux, 0x82 = swap) |
| 5-7 | 3 bytes | CHS del último sector |
| 8-11 | 4 bytes | LBA del primer sector |
| 12-15 | 4 bytes | Número total de sectores |

### 2.3. Limitaciones del MBR

Las limitaciones vienen directamente de la estructura de 16 bytes por entrada:

**Máximo 4 particiones primarias**: solo hay espacio para 4 entradas.

**Tamaño máximo 2 TiB**: el campo "número total de sectores" es de 4 bytes
(32 bits). Con sectores de 512 bytes: 2³² × 512 = 2 TiB.

```
2^32 sectores × 512 bytes/sector = 2,199,023,255,552 bytes ≈ 2 TiB
```

### 2.4. La solución: partición extendida + lógicas

Para superar el límite de 4 particiones, MBR permite convertir una de las 4
primarias en una **partición extendida**, que actúa como contenedor para
**particiones lógicas**:

```
┌─────────────────────────────────────────────────────────────┐
│                      DISCO MBR                              │
│                                                             │
│  ┌─────────┬─────────┬─────────────────────────────────┐    │
│  │ sda1    │ sda2    │ sda3 (extendida)                │    │
│  │primaria │primaria │ ┌───────┬───────┬───────┐       │    │
│  │ (boot)  │ (datos) │ │ sda5  │ sda6  │ sda7  │       │    │
│  │         │         │ │lógica │lógica │lógica │       │    │
│  │         │         │ └───────┴───────┴───────┘       │    │
│  └─────────┴─────────┴─────────────────────────────────┘    │
│                                                             │
│  2 primarias + 1 extendida (con 3 lógicas) = 5 particiones  │
│  Las lógicas empiezan en número 5                           │
└─────────────────────────────────────────────────────────────┘
```

**Reglas de MBR**:
- Máximo 4 primarias, **o** 3 primarias + 1 extendida
- Solo puede haber **una** extendida
- Las particiones lógicas se numeran a partir de 5
- Las lógicas están encadenadas (cada una tiene un puntero a la siguiente)

### 2.5. En nuestras VMs de lab

```bash
sudo fdisk -l /dev/vda
```

```
Disklabel type: dos                     ← "dos" = MBR

Device     Boot   Start      End  Sectors  Size Id  Type
/dev/vda1  *       2048 18872319 18870272    9G 83  Linux
/dev/vda2      18874366 20969471  2095106 1023M  5  Extended
/dev/vda5      18874368 20969471  2095104 1023M 82  Linux swap
```

El instalador de Debian creó: 1 primaria (root) + 1 extendida + 1 lógica (swap).
Usó MBR porque las plantillas de cloud images suelen usar MBR para máxima
compatibilidad.

---

## 3. GPT: GUID Partition Table

### 3.1. Estructura

GPT ocupa más espacio que MBR y tiene redundancia incorporada:

```
┌─────────────────────────────────────────────────────────────┐
│                      DISCO GPT                              │
│                                                             │
│  LBA 0:  Protective MBR                                     │
│          (MBR ficticio para compatibilidad)                  │
│                                                             │
│  LBA 1:  GPT Header                                         │
│          - Firma "EFI PART"                                  │
│          - GUID del disco                                    │
│          - Número de entradas de partición                   │
│          - CRC32 checksums                                   │
│                                                             │
│  LBA 2-33:  Partition Entry Array                            │
│             128 entradas × 128 bytes cada una                │
│                                                             │
│  ┌──────────┬──────────┬──────────┬──────────┐               │
│  │ Partición│ Partición│ Partición│   ...    │               │
│  │    1     │    2     │    3     │          │               │
│  └──────────┴──────────┴──────────┴──────────┘               │
│                                                             │
│  LBA -33 a -2:  Backup Partition Entry Array                 │
│  LBA -1:        Backup GPT Header                            │
│                 (copia al final del disco)                    │
└─────────────────────────────────────────────────────────────┘
```

### 3.2. Cada entrada de partición GPT

Cada entrada tiene 128 bytes (vs 16 en MBR):

| Campo | Tamaño | Contenido |
|-------|--------|-----------|
| Partition Type GUID | 16 bytes | Tipo de partición (ej: Linux filesystem) |
| Unique Partition GUID | 16 bytes | UUID único de esta partición |
| First LBA | 8 bytes | Sector de inicio |
| Last LBA | 8 bytes | Sector final |
| Attribute flags | 8 bytes | Flags (required, legacy BIOS bootable...) |
| Partition Name | 72 bytes | Nombre en UTF-16 (legible por humanos) |

### 3.3. Ventajas de GPT sobre MBR

| Aspecto | MBR | GPT |
|---------|-----|-----|
| Particiones máx. | 4 primarias (o 3+extendida) | 128 (estándar) |
| Tamaño máx. disco | 2 TiB | 9.4 ZiB (zetabytes) |
| Tamaño máx. partición | 2 TiB | 9.4 ZiB |
| Redundancia | Ninguna | Header + tabla duplicados al final |
| Checksums | Ninguno | CRC32 del header y entradas |
| Partición extendida | Necesaria para >4 | No existe (no hace falta) |
| Identificador | Disk ID (4 bytes) | GUID (16 bytes, universal) |
| Nombre de partición | No | Sí (UTF-16, 36 chars) |

### 3.4. Protective MBR

GPT incluye un MBR falso en el LBA 0 que marca todo el disco como una sola
partición de tipo 0xEE ("GPT Protective"). Esto sirve para:

```
┌────────────────────────────────────────────────────────┐
│  Protective MBR: compatibilidad con herramientas viejas │
│                                                        │
│  Si un programa antiguo (que no entiende GPT) lee      │
│  el sector 0, ve un MBR con una partición tipo 0xEE    │
│  que ocupa todo el disco.                              │
│                                                        │
│  Resultado: el programa no intenta modificar el disco   │
│  porque "ya está particionado". Sin el Protective MBR, │
│  un programa viejo podría sobrescribir la tabla GPT.    │
└────────────────────────────────────────────────────────┘
```

### 3.5. Tipos de partición GPT (GUIDs)

En vez del byte de tipo de MBR (0x83, 0x82...), GPT usa GUIDs completos:

| GUID | Tipo | Equivalente MBR |
|------|------|-----------------|
| `C12A7328-F81F-11D2-BA4B-00A0C93EC93B` | EFI System Partition | 0xEF |
| `0FC63DAF-8483-4772-8E79-3D69D8477DE4` | Linux filesystem | 0x83 |
| `0657FD6D-A4AB-43C4-84E5-0933C84B4F4F` | Linux swap | 0x82 |
| `E6D6D379-F507-44C2-A23C-238F2A3DF928` | Linux LVM | 0x8E |
| `A19D880F-05FC-4D3B-A006-743F0F84911E` | Linux RAID | 0xFD |

No necesitas memorizar los GUIDs — las herramientas (`fdisk`, `gdisk`, `parted`)
los gestionan con alias legibles.

---

## 4. Comparación directa

```
┌──────────────────────────────────────────────────────────┐
│              MBR vs GPT — RESUMEN                        │
│                                                          │
│  MBR (1983)                GPT (2000s)                   │
│  ─────────────             ─────────────                 │
│  512 bytes                 ~17 KiB + backup              │
│  4 particiones             128 particiones                │
│  2 TiB máximo              9.4 ZiB máximo                │
│  Sin redundancia           Header+tabla duplicados        │
│  Sin checksums             CRC32                          │
│  BIOS                      UEFI (y BIOS con truco)       │
│  Tipo: 1 byte (0x83)      Tipo: GUID (16 bytes)         │
│  Sin nombre                Nombre UTF-16                  │
│  Herramienta: fdisk        Herramienta: gdisk, fdisk     │
│                                                          │
│  ¿Cuándo MBR?             ¿Cuándo GPT?                  │
│  - Discos < 2 TiB         - Discos ≥ 2 TiB              │
│  - BIOS legacy             - UEFI                        │
│  - Compatibilidad          - Más de 4 particiones        │
│  - VMs de lab              - Producción moderna           │
│                            - Cualquier disco nuevo        │
└──────────────────────────────────────────────────────────┘
```

---

## 5. MBR y BIOS vs GPT y UEFI

### 5.1. El vínculo con el firmware

El tipo de tabla de particiones está relacionado (pero no estrictamente ligado)
al firmware de arranque:

```
BIOS (legacy):                    UEFI (moderno):
┌───────────────────────┐         ┌───────────────────────┐
│ 1. BIOS lee sector 0  │         │ 1. UEFI lee GPT header│
│    (los 512 bytes MBR) │         │    y partition table  │
│                       │         │                       │
│ 2. Ejecuta bootstrap  │         │ 2. Busca la ESP       │
│    code del MBR       │         │    (EFI System Part.) │
│    (446 bytes de code)│         │                       │
│                       │         │ 3. Ejecuta el .efi    │
│ 3. Bootstrap carga    │         │    del bootloader     │
│    GRUB stage 2       │         │    (GRUB, systemd-boot│
│                       │         │     etc.)             │
│ 4. GRUB carga kernel  │         │                       │
└───────────────────────┘         │ 4. Bootloader carga   │
                                  │    kernel             │
                                  └───────────────────────┘
```

### 5.2. Combinaciones posibles

| Firmware | Tabla de particiones | ¿Funciona? | Notas |
|----------|---------------------|------------|-------|
| BIOS | MBR | Sí | Combinación clásica |
| BIOS | GPT | Sí* | Necesita partición BIOS Boot (1 MiB) |
| UEFI | GPT | Sí | Combinación moderna estándar |
| UEFI | MBR | Sí* | Modo CSM (compatibilidad), no recomendado |

### 5.3. En nuestras VMs

Las VMs de QEMU/KVM por defecto usan firmware SeaBIOS (emulación de BIOS
legacy). Por eso las plantillas usan MBR. Si necesitaras UEFI, configurarías
OVMF en la VM:

```bash
# VM con UEFI (si lo necesitaras)
virt-install ... --boot uefi
```

Para los labs de B08, MBR es suficiente. Pero practicaremos GPT con `gdisk`
porque es el estándar en producción moderna.

---

## 6. Cuándo usar cada uno

### 6.1. Usa MBR cuando

- El sistema arranca con BIOS legacy
- Todos los discos son menores a 2 TiB
- Necesitas máxima compatibilidad con sistemas antiguos
- Labs de aprendizaje donde la simplicidad ayuda

### 6.2. Usa GPT cuando

- El sistema arranca con UEFI
- Algún disco es mayor a 2 TiB
- Necesitas más de 4 particiones
- Es un sistema de producción nuevo
- Quieres redundancia (backup header/table)

### 6.3. Regla práctica

```
¿Disco nuevo en 2024+?  →  GPT (salvo que haya razón específica para MBR)
¿Disco existente?       →  No cambiar sin necesidad
¿VM de lab?             →  El que venga (MBR por defecto está bien)
¿Disco extra para lab?  →  Cualquiera — practicar ambos
```

---

## 7. Identificar el tipo en un disco

### 7.1. Con fdisk -l

```bash
sudo fdisk -l /dev/vda
```

Buscar la línea `Disklabel type`:

```
Disklabel type: dos    ← MBR
# o
Disklabel type: gpt    ← GPT
```

### 7.2. Con lsblk

```bash
lsblk -o NAME,PTTYPE /dev/vda
```

```
NAME PTTYPE
vda  dos        ← MBR
├─vda1
├─vda2
└─vda5
```

### 7.3. Con blkid

```bash
sudo blkid -o value -s PTTYPE /dev/vda
# dos     ← MBR
# gpt     ← GPT
```

### 7.4. Con gdisk

```bash
sudo gdisk -l /dev/vda
```

```
# Si es MBR:
Partition table scan:
  MBR: MBR only
  GPT: not present

# Si es GPT:
Partition table scan:
  MBR: protective
  GPT: present
```

---

## 8. Errores comunes

### Error 1: crear más de 4 primarias en MBR

```bash
# ✗ Intentar crear una 5ª partición primaria
sudo fdisk /dev/vdb
# Command: n
# Partition type: p
# "All primary partitions are in use"

# ✓ Crear 3 primarias + 1 extendida + N lógicas
# o
# ✓ Usar GPT si necesitas más de 4 particiones
```

### Error 2: crear MBR en un disco mayor a 2 TiB

```bash
# ✗ fdisk con MBR en un disco de 4 TiB
sudo fdisk /dev/sda    # disco de 4TB
# "WARNING: The size of this disk is 4.0 TB. DOS partition table
#  format cannot be used on drives for volumes larger than 2199023255040
#  bytes for 512-byte sectors."

# ✓ Usar GPT
sudo gdisk /dev/sda
# o
sudo parted /dev/sda mklabel gpt
```

### Error 3: confundir "dos" con MBR

```bash
# fdisk y lsblk muestran "dos" — esto ES MBR
# "dos" se refiere a "DOS partition table" = MBR
# Son sinónimos:
#   dos = MBR = msdos = "tabla de particiones clásica"

# No es el sistema operativo DOS, es el formato de tabla
```

### Error 4: cambiar de MBR a GPT sin precaución

```bash
# ✗ Convertir un disco con datos de MBR a GPT sin backup
sudo gdisk /dev/sda
# Command: w
# "Do you want to proceed? (Y/N)"
# Si dices Y, puedes perder datos

# ✓ En discos con datos: usar gdisk con cuidado (hace conversión)
# ✓ En discos de lab vacíos: convertir libremente
sudo parted /dev/vdb mklabel gpt    # borra tabla MBR, crea GPT
```

### Error 5: asumir que GPT requiere UEFI

```bash
# GPT funciona con BIOS, pero necesita una partición BIOS Boot:
# Partición de 1 MiB, tipo "BIOS boot partition" (ef02 en gdisk)
# GRUB la usa para almacenar su stage 2

# En VMs con SeaBIOS + GPT:
sudo gdisk /dev/vdb
# Crear partición 1: 1MiB, tipo ef02 (BIOS boot)
# Crear partición 2: resto del disco, tipo 8300 (Linux filesystem)
```

---

## 9. Cheatsheet

```bash
# ── Identificar tipo de tabla ────────────────────────────────────
sudo fdisk -l /dev/vda | grep "Disklabel"    # dos = MBR, gpt = GPT
lsblk -o NAME,PTTYPE /dev/vda               # PTTYPE column
sudo blkid -o value -s PTTYPE /dev/vda       # solo el tipo
sudo gdisk -l /dev/vda                       # scan detallado

# ── Crear tabla de particiones ───────────────────────────────────
# MBR:
sudo parted /dev/vdb mklabel msdos
# o dentro de fdisk: comando 'o'

# GPT:
sudo parted /dev/vdb mklabel gpt
# o dentro de gdisk: comando 'o'

# ── Límites MBR ──────────────────────────────────────────────────
# Particiones: 4 primarias (o 3 primarias + 1 extendida con N lógicas)
# Tamaño máx: 2 TiB
# Numeración: primarias 1-4, lógicas 5+

# ── Límites GPT ──────────────────────────────────────────────────
# Particiones: 128 (estándar, configurable)
# Tamaño máx: 9.4 ZiB
# Sin extendidas/lógicas — todas son "primarias"
# Incluye backup al final del disco

# ── Tipos de partición comunes ───────────────────────────────────
# MBR:  83 = Linux, 82 = swap, 8e = LVM, fd = RAID, ef = EFI
# GPT:  8300 = Linux fs, 8200 = swap, 8e00 = LVM, fd00 = RAID,
#       ef00 = EFI, ef02 = BIOS boot

# ── Firmware + tabla ─────────────────────────────────────────────
# BIOS + MBR:  clásico
# BIOS + GPT:  necesita partición BIOS boot (ef02)
# UEFI + GPT:  moderno estándar
# UEFI + MBR:  modo CSM (compatibilidad)
```

---

## 10. Ejercicios

### Ejercicio 1: identificar la tabla en tu VM

1. Ejecuta `sudo fdisk -l /dev/vda` — ¿es MBR (`dos`) o GPT?
2. Ejecuta `lsblk -o NAME,PTTYPE /dev/vda` para confirmar
3. Observa la tabla de particiones:
   - ¿Cuántas particiones primarias hay?
   - ¿Hay partición extendida? ¿Y lógicas?
   - ¿Cuál es la partición de boot (`*`)?
4. Calcula: con MBR, ¿cuántas particiones más podrías crear en `vda`?

**Pregunta de reflexión**: el disco `vda` usa MBR con 2 primarias (una normal +
una extendida) y 1 lógica (swap). Si necesitaras agregar 3 particiones más,
¿podrías hacerlo con MBR? ¿Cuántas serían primarias y cuántas lógicas?

### Ejercicio 2: crear ambas tablas

1. Crea una tabla MBR en `vdb`:
   ```
   sudo parted /dev/vdb mklabel msdos
   ```
2. Verifica con `sudo fdisk -l /dev/vdb` — confirma `Disklabel type: dos`
3. Ahora crea una tabla GPT en `vdb`:
   ```
   sudo parted /dev/vdb mklabel gpt
   ```
4. Verifica con `sudo fdisk -l /dev/vdb` — confirma `Disklabel type: gpt`
5. Compara las salidas: ¿qué diferencias ves en los campos?

**Pregunta de reflexión**: al ejecutar `mklabel gpt`, parted advierte que se
borrarán todos los datos. ¿Qué pasó exactamente con la tabla MBR que habías
creado? ¿Se borró todo el disco o solo se sobrescribió el primer sector?

### Ejercicio 3: límite de 2 TiB

1. Crea un disco virtual grande (solo para este ejercicio):
   ```
   sudo qemu-img create -f qcow2 /tmp/big-disk.qcow2 3T
   ```
   (Gracias a thin provisioning, solo ocupa ~200KB)
2. Asocia a un loop device:
   ```
   sudo losetup /dev/loop0 /tmp/big-disk.qcow2
   ```
3. Intenta crear tabla MBR:
   ```
   sudo fdisk /dev/loop0
   ```
   — observa el warning sobre el tamaño
4. Crea tabla GPT en su lugar:
   ```
   sudo gdisk /dev/loop0
   ```
5. Limpia:
   ```
   sudo losetup -d /dev/loop0
   rm /tmp/big-disk.qcow2
   ```

**Pregunta de reflexión**: el archivo qcow2 ocupa ~200KB aunque representa un
disco de 3 TiB. ¿Podría un disco qcow2 tener tabla MBR si el tamaño virtual
excede 2 TiB? ¿Qué pasaría con el espacio más allá de 2 TiB?
