# Btrfs

## Índice

1. [Qué es Btrfs](#1-qué-es-btrfs)
2. [Crear un filesystem Btrfs](#2-crear-un-filesystem-btrfs)
3. [Subvolúmenes](#3-subvolúmenes)
4. [Snapshots](#4-snapshots)
5. [RAID integrado](#5-raid-integrado)
6. [Compresión transparente](#6-compresión-transparente)
7. [Otras características](#7-otras-características)
8. [Btrfs en el contexto del curso](#8-btrfs-en-el-contexto-del-curso)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Qué es Btrfs

Btrfs (B-tree filesystem, pronunciado "butter FS") es un filesystem moderno
con funcionalidad avanzada integrada: subvolúmenes, snapshots, RAID,
compresión y checksums de datos. Fue diseñado para ser el "ZFS de Linux".

```
┌────────────────────────────────────────────────────────────┐
│  Filosofía de cada filesystem:                             │
│                                                            │
│  ext4:   "Almaceno archivos de forma confiable"            │
│          → simple, robusto, probado                        │
│                                                            │
│  XFS:    "Almaceno archivos enormes muy rápido"            │
│          → rendimiento, paralelismo, escala                │
│                                                            │
│  Btrfs:  "Soy un gestor de almacenamiento completo"       │
│          → snapshots, RAID, compresión, checksums,         │
│            subvolúmenes — todo integrado en el filesystem  │
└────────────────────────────────────────────────────────────┘
```

### Dónde encontrar Btrfs

| Distribución | Estado |
|-------------|--------|
| SUSE/openSUSE | **Default para /** desde 2014 |
| Fedora | **Default para /** desde Fedora 33 (2020) |
| Debian/Ubuntu | Disponible, no default |
| RHEL/AlmaLinux | **Eliminado** (no soportado) |
| Arch Linux | Disponible, popular |

### Características principales

| Característica | Valor |
|----------------|-------|
| Copy-on-Write (CoW) | Sí — fundamento de todo |
| Subvolúmenes | Sí — como directorios montables |
| Snapshots | Sí — instantáneos, eficientes |
| RAID integrado | 0, 1, 10 (5/6 experimentales) |
| Compresión | zlib, lzo, zstd |
| Checksums de datos | Sí (CRC32C por defecto) |
| Online resize | Crecer y encoger |
| Deduplicación | Offline (herramientas externas) |
| Send/Receive | Sí — transferir snapshots incrementales |
| Tamaño máx filesystem | 16 EiB |

---

## 2. Crear un filesystem Btrfs

### 2.1. Instalar herramientas

```bash
# Debian/Ubuntu
sudo apt install btrfs-progs

# Fedora (ya instalado)
sudo dnf install btrfs-progs

# AlmaLinux/RHEL — Btrfs NO está disponible
```

### 2.2. mkfs.btrfs básico

```bash
sudo mkfs.btrfs /dev/vdb1
```

```
btrfs-progs v6.6
See https://btrfs.readthedocs.io for more information.

NOTE: several default settings have changed in version 5.15, please make sure
      this does not affect your deployments:
      ...

Label:              (none)
UUID:               a1b2c3d4-e5f6-7890-abcd-ef1234567890
Node size:          16384
Sector size:        4096
Filesystem size:    1.00GiB
Block group profiles:
  Data:             single            8.00MiB
  Metadata:         DUP              51.19MiB
  System:           DUP               8.00MiB
SSD detected:       no
Zoned device:       no
Incompat features:  extref, skinny-metadata, no-holes, free-space-tree
Runtime features:
Checksum:           crc32c
Number of devices:  1
Devices:
   ID        SIZE  PATH
    1     1.00GiB  /dev/vdb1
```

### 2.3. Opciones comunes

```bash
# Con label
sudo mkfs.btrfs -L "datos" /dev/vdb1

# Forzar sobreescritura
sudo mkfs.btrfs -f /dev/vdb1

# Con compresión por defecto (se configura al montar, no al crear)

# Sobre múltiples dispositivos (RAID)
sudo mkfs.btrfs -d raid1 -m raid1 /dev/vdb /dev/vdc
```

### 2.4. Metadata DUP

Observa en la salida que Metadata usa perfil `DUP` — Btrfs duplica los
metadatos automáticamente dentro del mismo disco:

```
Metadata:  DUP    ← duplicado para protección
Data:      single ← sin duplicar (es responsabilidad del usuario)
```

Esto es una capa de seguridad adicional que ext4 y XFS no tienen.

---

## 3. Subvolúmenes

### 3.1. Concepto

Un subvolumen es como un filesystem dentro del filesystem. Tiene su propio
árbol de archivos y se puede montar independientemente:

```
┌──────────────────────────────────────────────────────────┐
│  Btrfs sin subvolúmenes:    Btrfs con subvolúmenes:      │
│                                                          │
│  /dev/vdb1                  /dev/vdb1                    │
│  └── / (todo junto)        ├── @ (subvol para /)        │
│      ├── home/              ├── @home (subvol para /home)│
│      ├── var/               └── @snapshots               │
│      └── etc/                                            │
│                             Cada uno se monta por        │
│                             separado y puede tener       │
│                             snapshots independientes     │
└──────────────────────────────────────────────────────────┘
```

### 3.2. Crear subvolúmenes

```bash
# Montar el filesystem
sudo mount /dev/vdb1 /mnt

# Crear subvolúmenes
sudo btrfs subvolume create /mnt/@root
sudo btrfs subvolume create /mnt/@home
sudo btrfs subvolume create /mnt/@snapshots

# Listar subvolúmenes
sudo btrfs subvolume list /mnt
```

```
ID 256 gen 5 top level 5 path @root
ID 257 gen 5 top level 5 path @home
ID 258 gen 5 top level 5 path @snapshots
```

### 3.3. Montar subvolúmenes específicos

```bash
# Desmontar el filesystem general
sudo umount /mnt

# Montar un subvolumen específico
sudo mount -o subvol=@root /dev/vdb1 /mnt
sudo mount -o subvol=@home /dev/vdb1 /mnt/home

# O por ID
sudo mount -o subvolid=256 /dev/vdb1 /mnt
```

### 3.4. Subvolumen por defecto

```bash
# Ver el subvolumen por defecto (el que se monta sin -o subvol)
sudo btrfs subvolume get-default /mnt
# ID 5 (FS_TREE)    ← el root del filesystem

# Cambiar el default
sudo btrfs subvolume set-default 256 /mnt
# Ahora mount /dev/vdb1 /mnt montará @root automáticamente
```

### 3.5. Eliminar subvolúmenes

```bash
sudo btrfs subvolume delete /mnt/@snapshots
```

### 3.6. Subvolúmenes en fstab

```
# /etc/fstab
UUID=a1b2c3d4-...  /       btrfs  subvol=@root,defaults     0 0
UUID=a1b2c3d4-...  /home   btrfs  subvol=@home,defaults     0 0
```

Nota: el mismo UUID para ambas entradas — es el mismo filesystem Btrfs, solo
se montan diferentes subvolúmenes.

---

## 4. Snapshots

### 4.1. Concepto

Un snapshot Btrfs es una copia instantánea de un subvolumen en un momento dado.
Gracias a Copy-on-Write, la creación es **instantánea** y no ocupa espacio
adicional hasta que los datos divergen:

```
┌────────────────────────────────────────────────────────────┐
│  Copy-on-Write (CoW):                                      │
│                                                            │
│  Antes del snapshot:                                       │
│  @root ──► [bloque A] [bloque B] [bloque C]               │
│                                                            │
│  Después del snapshot:                                     │
│  @root     ──► [bloque A] [bloque B] [bloque C]           │
│  @snap-day1 ──► ↑           ↑           ↑                 │
│                 (comparten los mismos bloques)              │
│                                                            │
│  Al modificar un archivo en @root:                         │
│  @root     ──► [bloque A'] [bloque B] [bloque C]          │
│  @snap-day1 ──► [bloque A]  ↑           ↑                 │
│                 (solo el bloque modificado se duplica)      │
└────────────────────────────────────────────────────────────┘
```

### 4.2. Crear snapshots

```bash
# Snapshot read-only (recomendado para backups)
sudo btrfs subvolume snapshot -r /mnt/@root /mnt/@snapshots/root-2024-03-20

# Snapshot read-write (para experimentar)
sudo btrfs subvolume snapshot /mnt/@root /mnt/@snapshots/root-test
```

| Flag | Tipo | Uso |
|------|------|-----|
| `-r` | Read-only | Backups, send/receive |
| (ninguno) | Read-write | Experimentación, rollback |

### 4.3. Listar snapshots

```bash
sudo btrfs subvolume list -s /mnt
```

El flag `-s` muestra solo snapshots (los que tienen un "parent UUID").

### 4.4. Restaurar desde snapshot

Para "revertir" al estado del snapshot:

```bash
# Opción A: renombrar (rápido)
sudo umount /mnt
sudo mount /dev/vdb1 /mnt                          # montar sin subvol
sudo mv /mnt/@root /mnt/@root-broken                # mover el actual
sudo btrfs subvolume snapshot /mnt/@snapshots/root-2024-03-20 /mnt/@root
sudo umount /mnt
sudo mount -o subvol=@root /dev/vdb1 /mnt           # montar el restaurado

# Opción B: cambiar subvolumen por defecto
sudo btrfs subvolume set-default ID_DEL_SNAPSHOT /mnt
```

### 4.5. Eliminar snapshots

```bash
sudo btrfs subvolume delete /mnt/@snapshots/root-2024-03-20
```

Los snapshots son subvolúmenes — se eliminan con `subvolume delete`.

### 4.6. Snapshots vs snapshots de VM

```
Snapshots Btrfs:                    Snapshots virsh:
  ├── Solo datos del filesystem       ├── Todo: RAM + disco
  ├── Instantáneos (CoW)              ├── Instantáneos (qcow2 COW)
  ├── Dentro de la VM/host            ├── Desde el hipervisor
  ├── Granularidad: por subvolumen    ├── Granularidad: toda la VM
  └── Eficiente para rollback de /    └── Eficiente para rollback completo
```

---

## 5. RAID integrado

### 5.1. Concepto

Btrfs puede hacer RAID sin `mdadm`. El RAID se gestiona a nivel del filesystem:

```bash
# RAID1 con 2 discos
sudo mkfs.btrfs -d raid1 -m raid1 /dev/vdb /dev/vdc
```

| Flag | Qué configura |
|------|---------------|
| `-d raid1` | Perfil RAID para datos |
| `-m raid1` | Perfil RAID para metadatos |

### 5.2. Perfiles RAID soportados

| Perfil | Discos mín. | Tolerancia | Estado |
|--------|-------------|------------|--------|
| `single` | 1 | 0 | Estable |
| `dup` | 1 | 0 (duplica en mismo disco) | Estable |
| `raid0` | 2 | 0 | Estable |
| `raid1` | 2 | 1 disco | Estable |
| `raid10` | 4 | 1 por grupo | Estable |
| `raid5` | 3 | 1 disco | **Inestable** |
| `raid6` | 4 | 2 discos | **Inestable** |

**RAID5/6 en Btrfs tiene bugs conocidos de pérdida de datos**. No usar en
producción. Para RAID5/6, usar `mdadm` (C04).

### 5.3. Montar RAID Btrfs

```bash
# Montar (basta especificar un dispositivo)
sudo mount /dev/vdb /mnt

# Ver dispositivos del filesystem
sudo btrfs filesystem show /mnt
```

```
Label: none  uuid: a1b2c3d4-...
        Total devices 2 FS bytes used 256.00KiB
        devid    1 size 2.00GiB used 240.75MiB path /dev/vdb
        devid    2 size 2.00GiB used 240.75MiB path /dev/vdc
```

### 5.4. Agregar y quitar dispositivos

```bash
# Agregar un disco
sudo btrfs device add /dev/vdd /mnt

# Rebalancear datos (distribuir entre todos los discos)
sudo btrfs balance start /mnt

# Quitar un disco (mueve datos a los otros primero)
sudo btrfs device remove /dev/vdc /mnt
```

---

## 6. Compresión transparente

### 6.1. Activar compresión

La compresión se activa al montar:

```bash
sudo mount -o compress=zstd /dev/vdb1 /mnt
```

| Algoritmo | Velocidad | Ratio | Uso |
|-----------|-----------|-------|-----|
| `lzo` | Muy rápida | Bajo | CPU limitada |
| `zlib` | Lenta | Alto | Máxima compresión |
| `zstd` | Rápida | Alto | **Recomendado** (mejor balance) |
| `zstd:N` | Configurable (1-15) | Variable | `zstd:3` es buen default |

### 6.2. Compresión en fstab

```
UUID=a1b2c3d4-...  /data  btrfs  compress=zstd,defaults  0 0
```

### 6.3. Verificar compresión

```bash
# Ver ratio de compresión
sudo btrfs filesystem du -s /mnt
```

```
     Total   Exclusive  Set shared  Filename
   1.00GiB   600.00MiB           -  /mnt
```

Si `Total` > `Exclusive`, la compresión (o CoW) está ahorrando espacio.

```bash
# Compresión retroactiva (archivos existentes)
sudo btrfs filesystem defragment -r -czstd /mnt
```

---

## 7. Otras características

### 7.1. Checksums de datos

Btrfs verifica la integridad de cada bloque de datos con checksums:

```
ext4/XFS:                           Btrfs:
  Escribir datos → disco              Escribir datos + checksum → disco
  Leer datos ← disco                  Leer datos + verificar checksum ← disco
  ¿Datos corruptos? No sabe.          ¿Datos corruptos? Lo detecta.
                                       Con RAID1: auto-repara.
```

### 7.2. Send/Receive

Transferir snapshots incrementales entre filesystems:

```bash
# Enviar un snapshot (read-only) a otro filesystem
sudo btrfs send /mnt/@snapshots/root-day1 | sudo btrfs receive /backup/

# Enviar solo los cambios desde el snapshot anterior
sudo btrfs send -p /mnt/@snapshots/root-day1 \
  /mnt/@snapshots/root-day2 | sudo btrfs receive /backup/
```

### 7.3. Scrub

Verificar la integridad de todos los datos en el filesystem:

```bash
# Iniciar scrub (puede tardar en discos grandes)
sudo btrfs scrub start /mnt

# Ver estado
sudo btrfs scrub status /mnt
```

### 7.4. Filesystem usage

```bash
sudo btrfs filesystem usage /mnt
```

```
Overall:
    Device size:                   2.00GiB
    Device allocated:            481.50MiB
    Device unallocated:            1.53GiB
    Used:                        256.00KiB
    Free (estimated):              1.52GiB
```

---

## 8. Btrfs en el contexto del curso

```
┌────────────────────────────────────────────────────────────┐
│         BTRFS EN ESTE CURSO                                │
│                                                            │
│  Estado: CONOCIMIENTO, no herramienta principal            │
│                                                            │
│  Lo que debes saber:                                       │
│  ✓ Qué es y qué problemas resuelve                        │
│  ✓ Crear un filesystem y montarlo                          │
│  ✓ Concepto de subvolúmenes y snapshots                    │
│  ✓ Que tiene RAID integrado (y que RAID5/6 es inestable)   │
│  ✓ Compresión transparente con zstd                        │
│                                                            │
│  Lo que NO necesitas para B08:                             │
│  ✗ Usar Btrfs como filesystem principal en los labs        │
│  ✗ Dominar todas las opciones de balance/scrub             │
│  ✗ Btrfs RAID como alternativa a mdadm                     │
│                                                            │
│  Para los labs de B08:                                     │
│  - ext4 en VM Debian (T01)                                 │
│  - XFS en VM AlmaLinux (T02)                               │
│  - Btrfs: entender conceptualmente + 1 ejercicio práctico  │
│                                                            │
│  Nota: RHEL/AlmaLinux NO soporta Btrfs.                   │
│  Solo puedes practicarlo en la VM Debian.                  │
└────────────────────────────────────────────────────────────┘
```

---

## 9. Errores comunes

### Error 1: usar RAID5/6 de Btrfs en producción

```bash
# ✗ RAID5/6 de Btrfs tiene bugs de pérdida de datos
sudo mkfs.btrfs -d raid5 /dev/vdb /dev/vdc /dev/vdd
# ¡PELIGRO! Conocido como "write hole" — puede corromper datos

# ✓ Para RAID5/6 usar mdadm (C04)
sudo mdadm --create /dev/md0 --level=5 --raid-devices=3 /dev/vd{b,c,d}
```

### Error 2: quedarse sin espacio con metadatos DUP

```bash
# Btrfs puede reportar "no space left" aunque df muestre espacio libre
# Esto ocurre cuando los metadatos se llenan (están duplicados, ocupan más)

# Diagnóstico:
sudo btrfs filesystem usage /mnt
# Buscar: "Metadata, DUP: Size: X Used: X" — si Used ≈ Size, eso es el problema

# Solución:
sudo btrfs balance start -dusage=50 /mnt    # rebalancear datos
sudo btrfs balance start -musage=50 /mnt    # rebalancear metadatos
```

### Error 3: confundir snapshot con backup

```bash
# ✗ "Tengo snapshots, no necesito backup"
# Los snapshots están en el MISMO disco.
# Si el disco falla, pierdes datos Y snapshots.

# ✓ Snapshots = rollback rápido (error humano, actualización mala)
# ✓ Backup = protección contra fallo de hardware
# Combinar ambos: snapshot + send/receive a otro disco
```

### Error 4: no montar con compress si quieres compresión

```bash
# ✗ Crear filesystem y asumir que comprime
sudo mkfs.btrfs /dev/vdb1
sudo mount /dev/vdb1 /mnt
# Los datos NO se comprimen — la compresión no está activa

# ✓ Montar con compress
sudo mount -o compress=zstd /dev/vdb1 /mnt
# O agregar a fstab: compress=zstd en las opciones
```

### Error 5: intentar Btrfs en AlmaLinux/RHEL

```bash
# ✗ RHEL eliminó Btrfs
sudo mkfs.btrfs /dev/vdb1
# bash: mkfs.btrfs: command not found
# El paquete btrfs-progs no está disponible en los repos

# ✓ Usar Btrfs solo en Debian, Fedora, SUSE, Arch
# Para RHEL: ext4 o XFS
```

---

## 10. Cheatsheet

```bash
# ── Crear filesystem ────────────────────────────────────────────
sudo mkfs.btrfs /dev/vdb1                        # básico
sudo mkfs.btrfs -L "label" /dev/vdb1             # con label
sudo mkfs.btrfs -f /dev/vdb1                     # forzar sobreescritura
sudo mkfs.btrfs -d raid1 -m raid1 /dev/vdb /dev/vdc  # RAID1

# ── Montar ──────────────────────────────────────────────────────
sudo mount /dev/vdb1 /mnt                        # normal
sudo mount -o compress=zstd /dev/vdb1 /mnt       # con compresión
sudo mount -o subvol=@root /dev/vdb1 /mnt        # subvolumen específico
sudo mount -o subvolid=256 /dev/vdb1 /mnt        # por ID

# ── Subvolúmenes ────────────────────────────────────────────────
sudo btrfs subvolume create /mnt/@nombre          # crear
sudo btrfs subvolume list /mnt                    # listar
sudo btrfs subvolume delete /mnt/@nombre          # eliminar
sudo btrfs subvolume get-default /mnt             # ver default
sudo btrfs subvolume set-default ID /mnt          # cambiar default

# ── Snapshots ───────────────────────────────────────────────────
sudo btrfs subvolume snapshot -r /mnt/@src /mnt/@snap     # read-only
sudo btrfs subvolume snapshot /mnt/@src /mnt/@snap-rw     # read-write
sudo btrfs subvolume list -s /mnt                          # listar snapshots
sudo btrfs subvolume delete /mnt/@snap                     # eliminar

# ── RAID ────────────────────────────────────────────────────────
sudo btrfs device add /dev/vdd /mnt               # agregar disco
sudo btrfs device remove /dev/vdc /mnt             # quitar disco
sudo btrfs balance start /mnt                      # rebalancear
sudo btrfs filesystem show /mnt                    # ver dispositivos

# ── Inspección ──────────────────────────────────────────────────
sudo btrfs filesystem show /mnt                    # dispositivos y uso
sudo btrfs filesystem usage /mnt                   # uso detallado
sudo btrfs filesystem du -s /mnt                   # tamaño con CoW
sudo btrfs filesystem df /mnt                      # espacio por tipo

# ── Mantenimiento ───────────────────────────────────────────────
sudo btrfs scrub start /mnt                        # verificar integridad
sudo btrfs scrub status /mnt                       # estado del scrub
sudo btrfs balance start -dusage=50 /mnt           # rebalancear
sudo btrfs filesystem defragment -r -czstd /mnt    # defrag + comprimir

# ── Label y UUID ────────────────────────────────────────────────
sudo btrfs filesystem label /mnt                   # ver label
sudo btrfs filesystem label /mnt "nuevo"           # cambiar label
# UUID: usar btrfstune -u /dev/vdb1 (desmontado)
```

---

## 11. Ejercicios

### Ejercicio 1: crear Btrfs y explorar

1. En tu VM **Debian**, crea un filesystem Btrfs en `/dev/vdb`:
   `sudo mkfs.btrfs -f -L "lab-btrfs" /dev/vdb`
   (nota: en el disco completo, sin particionar — Btrfs lo permite)
2. Monta en `/mnt` y ejecuta:
   - `sudo btrfs filesystem show /mnt`
   - `sudo btrfs filesystem usage /mnt`
   - `df -h /mnt`
3. Crea algunos archivos y compara `df` antes y después

**Pregunta de reflexión**: Btrfs se creó directamente en `/dev/vdb` (sin
partición). ¿Por qué esto funciona? ¿Funcionaría con ext4 o XFS?
Pista: los tres lo permiten con `-F` o `-f`, pero ¿es buena práctica?

### Ejercicio 2: subvolúmenes y snapshots

1. Con el Btrfs del ejercicio anterior montado:
2. Crea 2 subvolúmenes: `@data` y `@snapshots`
3. Desmonta y remonta solo `@data`: `sudo mount -o subvol=@data /dev/vdb /mnt`
4. Crea archivos en `/mnt`: `file1.txt`, `file2.txt`, `file3.txt`
5. Crea un snapshot read-only:
   - Monta el root: `sudo mount /dev/vdb /mnt2`
   - `sudo btrfs subvolume snapshot -r /mnt2/@data /mnt2/@snapshots/data-snap1`
6. Modifica `file1.txt` y elimina `file2.txt` en `/mnt`
7. Compara el contenido de `/mnt` (actual) con `/mnt2/@snapshots/data-snap1/`
   (snapshot) — los archivos originales siguen en el snapshot

**Pregunta de reflexión**: el snapshot se creó instantáneamente y no duplicó
datos. Si el disco tiene 1.8G libres y los archivos ocupan 100M, ¿cuánto
espacio libre queda después del snapshot? ¿Y después de modificar 50M de datos?

### Ejercicio 3: compresión transparente

1. Monta el Btrfs con compresión: `sudo mount -o compress=zstd /dev/vdb /mnt`
2. Genera un archivo compresible (texto repetitivo):
   ```
   yes "This is a test line for compression" | head -1000000 > /mnt/compressible.txt
   ```
3. Genera un archivo incompresible (datos aleatorios):
   ```
   dd if=/dev/urandom of=/mnt/random.bin bs=1M count=50
   ```
4. Compara con `sudo btrfs filesystem du /mnt/compressible.txt` y
   `sudo btrfs filesystem du /mnt/random.bin`
5. Compara también con `ls -lh` — ¿los tamaños coinciden con el espacio real usado?

**Pregunta de reflexión**: el archivo de texto se comprime bien pero el de
datos aleatorios no. ¿La compresión transparente intenta comprimir ambos?
¿Cómo sabe Btrfs cuándo conviene comprimir y cuándo no? Investiga la opción
`compress-force` vs `compress`.
