# Crear y gestionar LVM

## Índice

1. [Flujo completo de creación](#flujo-completo-de-creación)
2. [Preparar los discos](#preparar-los-discos)
3. [pvcreate — Physical Volumes](#pvcreate--physical-volumes)
4. [vgcreate — Volume Groups](#vgcreate--volume-groups)
5. [lvcreate — Logical Volumes](#lvcreate--logical-volumes)
6. [Especificar tamaño en lvcreate](#especificar-tamaño-en-lvcreate)
7. [Inspección: pvs, vgs, lvs y *display](#inspección-pvs-vgs-lvs-y-display)
8. [Extender: vgextend y lvextend](#extender-vgextend-y-lvextend)
9. [Reducir: lvreduce y vgreduce](#reducir-lvreduce-y-vgreduce)
10. [Eliminar: lvremove, vgremove, pvremove](#eliminar-lvremove-vgremove-pvremove)
11. [pvmove — migrar datos entre PVs](#pvmove--migrar-datos-entre-pvs)
12. [Renombrar VGs y LVs](#renombrar-vgs-y-lvs)
13. [Errores comunes](#errores-comunes)
14. [Cheatsheet](#cheatsheet)
15. [Ejercicios](#ejercicios)

---

## Flujo completo de creación

Desde discos vacíos hasta un filesystem montado:

```
  Disco        PV           VG          LV         FS        Mount
┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐  ┌──────┐  ┌──────┐
│/dev/vdb│─►│pvcreate│─►│vgcreate│─►│lvcreate│─►│ mkfs │─►│mount │
│/dev/vdc│─►│        │─►│        │  │        │  │      │  │      │
└────────┘  └────────┘  └────────┘  └────────┘  └──────┘  └──────┘

Comandos en secuencia:
  1. pvcreate /dev/vdb /dev/vdc
  2. vgcreate vg0 /dev/vdb /dev/vdc
  3. lvcreate -L 20G -n lv_data vg0
  4. mkfs.ext4 /dev/vg0/lv_data
  5. mount /dev/vg0/lv_data /mnt/data
```

Cada paso depende del anterior — no se puede saltar ninguno.

---

## Preparar los discos

LVM puede usar discos completos o particiones. Ambos enfoques son válidos.

### Opción A: disco completo como PV

```bash
# Usar el disco entero — sin particionar
pvcreate /dev/vdb
```

Ventaja: más simple, aprovecha todo el espacio. Es lo más común en VMs y labs.

### Opción B: partición como PV

```bash
# Crear partición tipo LVM
fdisk /dev/vdb
# n → nueva partición → aceptar defaults
# t → cambiar tipo → 8e (Linux LVM)
# w → escribir

# Ahora usar la partición
pvcreate /dev/vdb1
```

Ventaja: permite mezclar LVM con particiones regulares en el mismo disco. El tipo `8e` (MBR) o `lvm` (GPT) documenta el propósito.

### ¿Particionar o no?

| Escenario | Recomendación |
|-----------|---------------|
| Disco dedicado a LVM | Disco completo — más simple |
| Disco compartido (boot + LVM) | Particiones — boot fuera de LVM |
| Examen RHCSA | Sigue las instrucciones — normalmente particiones |
| Labs | Disco completo — menos pasos |

---

## pvcreate — Physical Volumes

Inicializa un dispositivo de bloque para uso por LVM. Escribe la cabecera de metadatos LVM.

### Sintaxis

```bash
pvcreate [opciones] <dispositivo> [dispositivo...]
```

### Uso básico

```bash
# Un solo disco
pvcreate /dev/vdb

# Múltiples discos a la vez
pvcreate /dev/vdb /dev/vdc /dev/vdd

# Forzar (si el disco tenía datos previos)
pvcreate -f /dev/vdb
# Pide confirmación. Para scripts:
pvcreate -ff -y /dev/vdb
```

### Opciones útiles

| Opción | Efecto |
|--------|--------|
| `-f` | Forzar — sobreescribir firmas existentes |
| `-ff` | Doble forzar — sobreescribir incluso si hay VG |
| `-y` | Responder sí automáticamente |
| `--dataalignment` | Alineación de datos (para RAID hardware) |
| `-u <UUID>` | Asignar UUID específico (raro) |

### Verificar

```bash
# Listar PVs
pvs
#   PV         VG   Fmt  Attr PSize  PFree
#   /dev/vdb        lvm2 ---  1.00g  1.00g    ← sin VG aún
#   /dev/vdc        lvm2 ---  1.00g  1.00g

# Detalle de un PV
pvdisplay /dev/vdb
#   --- Physical volume ---
#   PV Name               /dev/vdb
#   VG Name                          ← vacío — no pertenece a un VG
#   PV Size               1.00 GiB
#   Allocatable           NO
#   PE Size               0          ← se define al unirse a un VG
#   Total PE              0
#   Free PE               0
#   Allocated PE          0
#   PV UUID               xxxxx-xxxx-xxxx-xxxx-xxxx-xxxx-xxxxxx
```

> **Predicción**: después de `pvcreate`, el PV aparece en `pvs` pero sin VG y con 0 PEs. Los PEs se crean cuando el PV se añade a un VG (que define el PE size).

---

## vgcreate — Volume Groups

Crea un pool de almacenamiento combinando uno o más PVs.

### Sintaxis

```bash
vgcreate [opciones] <nombre_vg> <pv> [pv...]
```

### Uso básico

```bash
# VG con un PV
vgcreate vg0 /dev/vdb

# VG con múltiples PVs
vgcreate vg0 /dev/vdb /dev/vdc

# VG con PE size específico (default: 4 MiB)
vgcreate -s 8M vg_big /dev/vdb /dev/vdc
```

### Opciones útiles

| Opción | Efecto |
|--------|--------|
| `-s <size>` | Tamaño del PE (4M, 8M, 16M, etc.) |
| `-p <N>` | Máximo de PVs permitidos |
| `-l <N>` | Máximo de LVs permitidos |

### Verificar

```bash
# Resumen de VGs
vgs
#   VG   #PV #LV #SN Attr   VSize VFree
#   vg0    2   0   0 wz--n- 1.99g 1.99g

# Detalle
vgdisplay vg0
#   --- Volume group ---
#   VG Name               vg0
#   System ID
#   Format                lvm2
#   VG Access             read/write
#   VG Size               1.99 GiB       ← tamaño total (con overhead)
#   PE Size               4.00 MiB       ← tamaño del extent
#   Total PE              510            ← PEs totales disponibles
#   Alloc PE / Size       0 / 0          ← nada asignado aún
#   Free  PE / Size       510 / 1.99 GiB
#   VG UUID               xxxxx-xxxx-...
```

> **Nota sobre el tamaño**: el VG es ligeramente menor que la suma de los PVs porque LVM reserva espacio al inicio de cada PV para metadatos. Con dos discos de 1 GiB, el VG reporta ~1.99 GiB.

---

## lvcreate — Logical Volumes

Asigna espacio del VG para crear un volumen lógico.

### Sintaxis

```bash
lvcreate [opciones] <nombre_vg>
```

### Uso básico

```bash
# LV de tamaño fijo
lvcreate -L 10G -n lv_data vg0

# LV usando porcentaje del espacio libre
lvcreate -l 100%FREE -n lv_data vg0

# LV especificando número de extents
lvcreate -l 2560 -n lv_data vg0    # 2560 × 4MiB = 10 GiB
```

### Opciones principales

| Opción | Efecto | Ejemplo |
|--------|--------|---------|
| `-L <size>` | Tamaño absoluto | `-L 10G`, `-L 500M` |
| `-l <extents>` | Tamaño en extents o porcentaje | `-l 2560`, `-l 50%FREE` |
| `-n <name>` | Nombre del LV | `-n lv_data` |
| `-i <stripes>` | Número de stripes (tipo striped) | `-i 2` |
| `-I <size>` | Tamaño del stripe | `-I 64k` |
| `-m <mirrors>` | Copias espejo | `-m 1` |

### Verificar

```bash
# Resumen de LVs
lvs
#   LV      VG   Attr       LSize  Pool Origin Data%  Meta%
#   lv_data vg0  -wi-a----- 10.00g

# Detalle
lvdisplay /dev/vg0/lv_data
#   --- Logical volume ---
#   LV Path                /dev/vg0/lv_data
#   LV Name                lv_data
#   VG Name                vg0
#   LV Size                10.00 GiB
#   Current LE             2560
#   Segments               1
#   Allocation             inherit
#   Read ahead sectors     auto
#   Block device           253:0

# Verificar que existe como dispositivo
ls -l /dev/vg0/lv_data
ls -l /dev/mapper/vg0-lv_data
```

### Después de crear: filesystem y montaje

```bash
# Crear filesystem
mkfs.ext4 /dev/vg0/lv_data

# Crear punto de montaje
mkdir -p /mnt/data

# Montar
mount /dev/vg0/lv_data /mnt/data

# Añadir a fstab para persistencia
echo '/dev/mapper/vg0-lv_data  /mnt/data  ext4  defaults  0  2' >> /etc/fstab
```

---

## Especificar tamaño en lvcreate

### Con -L (tamaño absoluto)

```bash
lvcreate -L 10G -n lv1 vg0       # 10 GiB
lvcreate -L 500M -n lv2 vg0      # 500 MiB
lvcreate -L 1T -n lv3 vg0        # 1 TiB
```

Sufijos: `K` (KiB), `M` (MiB), `G` (GiB), `T` (TiB).

### Con -l (extents o porcentaje)

```bash
# Número de extents
lvcreate -l 2560 -n lv1 vg0          # 2560 PEs × 4 MiB = 10 GiB

# Porcentaje del espacio LIBRE del VG
lvcreate -l 100%FREE -n lv1 vg0      # todo el espacio libre
lvcreate -l 50%FREE -n lv1 vg0       # mitad del espacio libre

# Porcentaje del tamaño TOTAL del VG
lvcreate -l 80%VG -n lv1 vg0         # 80% del VG completo

# Porcentaje de un PV específico
lvcreate -l 100%PVS -n lv1 vg0 /dev/vdb   # todo el espacio de /dev/vdb
```

### Comparación

```
┌─────────────────┬───────────────────────┬──────────────────────┐
│ Sintaxis        │ Ejemplo               │ Cuándo usar          │
├─────────────────┼───────────────────────┼──────────────────────┤
│ -L <size>       │ -L 10G                │ Tamaño exacto        │
│ -l <N>          │ -l 2560               │ Control preciso      │
│ -l N%FREE       │ -l 100%FREE           │ Usar todo lo libre   │
│ -l N%VG         │ -l 50%VG              │ Proporción del pool  │
│ -l N%PVS        │ -l 100%PVS /dev/vdb   │ Todo un PV           │
└─────────────────┴───────────────────────┴──────────────────────┘
```

> **Nota RHCSA**: en el examen, si te piden "crear un LV de exactamente 500 MiB", usa `-L 500M`. Si piden "usar todo el espacio restante", usa `-l 100%FREE`. Cuidado con `-L` vs `-l` (mayúscula vs minúscula).

---

## Inspección: pvs, vgs, lvs y *display

LVM ofrece dos niveles de inspección: resumen (`pvs`, `vgs`, `lvs`) y detalle (`pvdisplay`, `vgdisplay`, `lvdisplay`).

### Comandos de resumen

```bash
# PVs — una línea por PV
pvs
#   PV         VG   Fmt  Attr PSize    PFree
#   /dev/vdb   vg0  lvm2 a--  1020.00m  500.00m
#   /dev/vdc   vg0  lvm2 a--  1020.00m 1020.00m

# VGs — una línea por VG
vgs
#   VG   #PV #LV #SN Attr   VSize VFree
#   vg0    2   1   0 wz--n- 1.99g 1.48g

# LVs — una línea por LV
lvs
#   LV      VG   Attr       LSize   Pool Origin Data%
#   lv_data vg0  -wi-a----- 520.00m
```

### Columnas personalizadas

```bash
# pvs con columnas seleccionadas
pvs -o pv_name,pv_size,pv_free,vg_name
#   PV         PSize    PFree    VG
#   /dev/vdb   1020.00m  500.00m vg0

# lvs con más información
lvs -o lv_name,vg_name,lv_size,lv_attr,devices
#   LV      VG   LSize   Attr       Devices
#   lv_data vg0  520.00m -wi-a----- /dev/vdb(0)

# Columna "devices" muestra en qué PV y PE offset reside el LV
```

### Comandos de detalle

```bash
# pvdisplay — detalle completo del PV
pvdisplay /dev/vdb

# vgdisplay — detalle completo del VG
vgdisplay vg0

# lvdisplay — detalle completo del LV
lvdisplay /dev/vg0/lv_data

# Todos juntos
pvdisplay    # todos los PVs
vgdisplay    # todos los VGs
lvdisplay    # todos los LVs
```

### Mapa de asignación

```bash
# Ver qué PEs de un PV están en uso y por qué LV
pvdisplay -m /dev/vdb
#   --- Physical Segments ---
#   Physical extent 0 to 129:
#     Logical volume  /dev/vg0/lv_data
#     Logical extents 0 to 129
#   Physical extent 130 to 254:
#     FREE

# Mapa visual completo
lsblk
#   NAME            MAJ:MIN RM  SIZE RO TYPE MOUNTPOINT
#   vdb             252:16   0    1G  0 disk
#   └─vg0-lv_data   253:0    0  520M  0 lvm  /mnt/data
```

---

## Extender: vgextend y lvextend

### vgextend — añadir un PV al pool

```bash
# Nuevo disco disponible
pvcreate /dev/vdd

# Añadir al VG existente
vgextend vg0 /dev/vdd

# Verificar
vgs
# El VSize y VFree aumentaron
```

### lvextend — hacer crecer un LV

```bash
# Extender con tamaño adicional
lvextend -L +5G /dev/vg0/lv_data        # añadir 5 GiB

# Extender hasta un tamaño total
lvextend -L 20G /dev/vg0/lv_data         # crecer hasta 20 GiB

# Usar todo el espacio libre del VG
lvextend -l +100%FREE /dev/vg0/lv_data

# Extender LV + redimensionar filesystem en un solo paso
lvextend -L +5G --resizefs /dev/vg0/lv_data
```

**Ojo con `+` vs sin `+`**:

```
-L +5G   → AÑADIR 5 GiB al tamaño actual
-L 5G    → tamaño TOTAL de 5 GiB (puede ser una reducción si era más grande)

-l +100%FREE  → añadir todo el espacio libre
-l 100%FREE   → ERROR si el LV ya existe — ambiguo
```

### lvextend + filesystem

Extender el LV no extiende automáticamente el filesystem (a menos que uses `--resizefs`). Sin esa flag, necesitas un paso adicional:

```bash
# Paso 1: extender el LV
lvextend -L +5G /dev/vg0/lv_data

# Paso 2: extender el filesystem
# ext4:
resize2fs /dev/vg0/lv_data        # online, sin desmontar

# XFS:
xfs_growfs /mnt/data               # nota: usa el mountpoint, no el device
```

O todo en un paso:

```bash
# --resizefs hace ambos pasos
lvextend -L +5G --resizefs /dev/vg0/lv_data
```

> **Predicción**: `lvextend --resizefs` detecta el tipo de filesystem y ejecuta `resize2fs` o `xfs_growfs` automáticamente. Funciona **online** para ext4 y XFS — no necesitas desmontar para crecer.

---

## Reducir: lvreduce y vgreduce

### lvreduce — encoger un LV

Reducir es **peligroso** si no se hace en el orden correcto. El filesystem debe encogerse **antes** que el LV.

```
⚠ ORDEN CRÍTICO PARA REDUCIR:

  1. Desmontar el filesystem
  2. Verificar: fsck
  3. Reducir el FILESYSTEM primero (resize2fs)
  4. Reducir el LV después (lvreduce)

  Si reduces el LV antes del filesystem → PÉRDIDA DE DATOS
```

```bash
# Paso 1: desmontar
umount /mnt/data

# Paso 2: verificar (obligatorio para ext4 antes de resize)
e2fsck -f /dev/vg0/lv_data

# Paso 3: reducir filesystem a 5 GiB
resize2fs /dev/vg0/lv_data 5G

# Paso 4: reducir LV a 5 GiB
lvreduce -L 5G /dev/vg0/lv_data
# WARNING: Reducing active logical volume to 5.00 GiB
# THIS MAY DESTROY YOUR DATA (filesystem etc.)
# Do you really want to reduce lv_data? [y/n]: y

# Paso 5: remontar
mount /dev/vg0/lv_data /mnt/data
```

O con `--resizefs` (más seguro):

```bash
umount /mnt/data
lvreduce -L 5G --resizefs /dev/vg0/lv_data
# Ejecuta fsck, resize2fs y lvreduce en el orden correcto
mount /dev/vg0/lv_data /mnt/data
```

> **XFS no se puede reducir**. Si el LV tiene XFS, la única opción es crear un LV nuevo más pequeño, copiar los datos, y eliminar el original.

### vgreduce — retirar un PV del pool

```bash
# Verificar que el PV no tiene datos asignados
pvs
#   PV         VG   PSize    PFree
#   /dev/vdc   vg0  1020.00m 1020.00m   ← completamente libre

# Si tiene datos: moverlos primero
pvmove /dev/vdc    # mueve PEs a otros PVs del VG

# Retirar del VG
vgreduce vg0 /dev/vdc

# Limpiar la cabecera LVM del PV
pvremove /dev/vdc
```

---

## Eliminar: lvremove, vgremove, pvremove

El orden de eliminación es inverso al de creación:

```
Crear:   pvcreate → vgcreate → lvcreate
Eliminar: lvremove → vgremove → pvremove
```

### lvremove

```bash
# Desmontar primero
umount /mnt/data

# Eliminar el LV
lvremove /dev/vg0/lv_data
# Do you really want to remove active logical volume lv_data? [y/n]: y
#   Logical volume "lv_data" successfully removed

# Con -f para saltar confirmación
lvremove -f /dev/vg0/lv_data
```

### vgremove

```bash
# Eliminar el VG (elimina automáticamente todos sus LVs)
vgremove vg0
# ⚠ Esto elimina TODOS los LVs del VG

# Solo si los LVs están desmontados
```

### pvremove

```bash
# Limpiar cabecera LVM del dispositivo
pvremove /dev/vdb
#   Labels on physical volume "/dev/vdb" successfully wiped

# El disco queda libre para cualquier uso
```

### Eliminación completa en un solo flujo

```bash
# Desmontar todo lo del VG
umount /mnt/data
umount /mnt/logs

# Eliminar VG (y sus LVs)
vgremove -f vg0

# Limpiar PVs
pvremove /dev/vdb /dev/vdc /dev/vdd
```

---

## pvmove — migrar datos entre PVs

`pvmove` mueve los Physical Extents de un PV a otros PVs del mismo VG. Es útil para:
- Retirar un disco sin perder datos
- Migrar a un disco más rápido
- Balancear espacio entre PVs

```bash
# Mover todos los PEs de /dev/vdb a otros PVs del VG
pvmove /dev/vdb
# Esto puede tardar — mueve datos reales

# Mover a un PV específico
pvmove /dev/vdb /dev/vdd

# Ver progreso
pvmove -v /dev/vdb
```

### Flujo: reemplazar un disco

```bash
# 1. Añadir nuevo disco al VG
pvcreate /dev/vde
vgextend vg0 /dev/vde

# 2. Migrar datos del disco viejo
pvmove /dev/vdb /dev/vde

# 3. Verificar que el disco viejo está vacío
pvs /dev/vdb
# PFree debe ser igual a PSize

# 4. Retirar disco viejo
vgreduce vg0 /dev/vdb
pvremove /dev/vdb

# 5. Desconectar disco físicamente (si aplica)
```

> **Predicción**: `pvmove` funciona **online** — no necesitas desmontar los filesystems durante la migración. Los datos se mueven transparentemente mientras el sistema sigue en uso.

---

## Renombrar VGs y LVs

```bash
# Renombrar VG
vgrename vg0 vg_production
# ⚠ Actualizar /etc/fstab si referencia el VG viejo

# Renombrar LV
lvrename vg0 lv_data lv_database
# O con path completo
lvrename /dev/vg0/lv_data /dev/vg0/lv_database

# Después de renombrar: actualizar fstab
# Viejo: /dev/mapper/vg0-lv_data
# Nuevo: /dev/mapper/vg0-lv_database
```

> **Importante**: si el filesystem está montado con el nombre viejo en `/etc/fstab`, actualizar fstab **antes** de reiniciar. Si no, el sistema no encontrará el dispositivo y puede no arrancar.

---

## Errores comunes

### 1. Olvidar pvcreate antes de vgcreate

```bash
# ✗ Intentar crear VG sin PV
vgcreate vg0 /dev/vdb
#   Device /dev/vdb not found (or ignored by filtering)
#   (a veces funciona, pvcreate implícito, pero no siempre)

# ✓ Siempre hacer pvcreate primero
pvcreate /dev/vdb
vgcreate vg0 /dev/vdb
```

### 2. Confundir -L (tamaño) con -l (extents)

```bash
# ✗ -l con tamaño → error
lvcreate -l 10G -n lv_data vg0
#   Invalid argument for -l

# ✓ -L para tamaño, -l para extents o porcentaje
lvcreate -L 10G -n lv_data vg0
lvcreate -l 2560 -n lv_data vg0
lvcreate -l 100%FREE -n lv_data vg0
```

### 3. Reducir LV antes que el filesystem

```bash
# ✗ Reducir LV primero → DESTRUYE DATOS
lvreduce -L 5G /dev/vg0/lv_data    # corta el dispositivo
# El filesystem todavía espera el tamaño anterior → corrupción

# ✓ Reducir filesystem primero, luego LV
umount /mnt/data
e2fsck -f /dev/vg0/lv_data
resize2fs /dev/vg0/lv_data 5G
lvreduce -L 5G /dev/vg0/lv_data

# ✓✓ O usar --resizefs
lvreduce -L 5G --resizefs /dev/vg0/lv_data
```

### 4. Olvidar extender el filesystem después de lvextend

```bash
# ✗ Extender LV pero no el filesystem
lvextend -L +5G /dev/vg0/lv_data
# df -h todavía muestra el tamaño viejo

# ✓ Extender ambos
lvextend -L +5G /dev/vg0/lv_data
resize2fs /dev/vg0/lv_data       # ext4
xfs_growfs /mnt/data             # XFS

# ✓✓ O usar --resizefs
lvextend -L +5G --resizefs /dev/vg0/lv_data
```

### 5. Confundir +5G con 5G en lvextend

```bash
# ✗ Querer añadir 5G pero reducir a 5G total
lvextend -L 5G /dev/vg0/lv_data    # si ya era 10G → no cambia (ya es >5G)

# ✓ + para añadir, sin + para tamaño absoluto
lvextend -L +5G /dev/vg0/lv_data   # 10G + 5G = 15G
lvextend -L 15G /dev/vg0/lv_data   # tamaño total 15G
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│               Crear y gestionar LVM — Referencia rápida          │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  CREAR (orden: PV → VG → LV → mkfs → mount):                    │
│    pvcreate /dev/vdb /dev/vdc          inicializar PVs           │
│    vgcreate vg0 /dev/vdb /dev/vdc      crear VG                  │
│    lvcreate -L 10G -n lv_data vg0      crear LV                  │
│    lvcreate -l 100%FREE -n lv vg0      usar todo el espacio      │
│    mkfs.ext4 /dev/vg0/lv_data          crear filesystem          │
│    mount /dev/vg0/lv_data /mnt/data    montar                    │
│                                                                  │
│  INSPECCIONAR:                                                   │
│    pvs / pvdisplay                     Physical Volumes           │
│    vgs / vgdisplay                     Volume Groups              │
│    lvs / lvdisplay                     Logical Volumes            │
│    pvdisplay -m /dev/vdb               mapa de PEs               │
│                                                                  │
│  EXTENDER:                                                       │
│    vgextend vg0 /dev/vdd               añadir PV al pool         │
│    lvextend -L +5G /dev/vg0/lv         crecer LV (+5G)           │
│    lvextend -L +5G --resizefs ...      crecer LV + filesystem    │
│                                                                  │
│  REDUCIR (⚠ peligroso — orden importa):                         │
│    umount → e2fsck → resize2fs → lvreduce   orden manual         │
│    lvreduce -L 5G --resizefs ...            automático           │
│    XFS: NO se puede reducir                                      │
│                                                                  │
│  ELIMINAR (orden: LV → VG → PV):                                │
│    umount /mnt/data                                              │
│    lvremove /dev/vg0/lv_data                                     │
│    vgremove vg0                                                  │
│    pvremove /dev/vdb                                             │
│                                                                  │
│  MIGRAR:                                                         │
│    pvmove /dev/vdb               mover PEs a otros PVs (online)  │
│    pvmove /dev/vdb /dev/vde      mover a PV específico           │
│                                                                  │
│  RENOMBRAR:                                                      │
│    vgrename vg0 vg_new           renombrar VG                    │
│    lvrename vg0 lv_old lv_new    renombrar LV                    │
│    → actualizar /etc/fstab después                               │
│                                                                  │
│  -L vs -l:                                                       │
│    -L 10G      tamaño absoluto                                   │
│    -l 2560     número de extents                                 │
│    -l 100%FREE porcentaje del espacio libre                      │
│    +5G         AÑADIR  |  5G = tamaño TOTAL                      │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Crear un stack LVM completo

En tu VM de lab con discos `/dev/vdb` y `/dev/vdc` (1 GiB cada uno):

1. Inicializa ambos como PVs: `pvcreate /dev/vdb /dev/vdc`
2. Crea un VG llamado `vg_lab`: `vgcreate vg_lab /dev/vdb /dev/vdc`
3. Verifica: `vgs` — ¿cuánto espacio total tiene el VG?
4. Crea un LV de 500 MiB: `lvcreate -L 500M -n lv_web vg_lab`
5. Crea un segundo LV con el 50% del espacio libre: `lvcreate -l 50%FREE -n lv_db vg_lab`
6. Verifica con `lvs` — ¿cuántos MiB tiene cada LV?
7. Formatea: `mkfs.ext4 /dev/vg_lab/lv_web` y `mkfs.xfs /dev/vg_lab/lv_db`
8. Monta en `/mnt/web` y `/mnt/db`
9. Ejecuta `lsblk` y dibuja la jerarquía que ves

> **Pregunta de reflexión**: ¿por qué el espacio libre reportado por `vgs` antes del paso 4 no es exactamente 2 GiB si sumaste dos discos de 1 GiB?

### Ejercicio 2: Extender y reducir

Continuando del ejercicio anterior:

1. Verifica espacio libre: `vgs` y `df -h /mnt/web`
2. Extiende `lv_web` en 200 MiB con resize del filesystem: `lvextend -L +200M --resizefs /dev/vg_lab/lv_web`
3. Verifica: `lvs` y `df -h /mnt/web` — ¿ambos reportan el nuevo tamaño?
4. Ahora reduce `lv_web` a 400 MiB:
   ```bash
   umount /mnt/web
   lvreduce -L 400M --resizefs /dev/vg_lab/lv_web
   mount /dev/vg_lab/lv_web /mnt/web
   ```
5. ¿Qué pasa si intentas reducir `lv_db` (XFS)? Inténtalo y observa el error
6. Verifica el estado final: `pvs`, `vgs`, `lvs`

> **Pregunta de reflexión**: ¿por qué `lvextend --resizefs` funciona online (sin desmontar) pero `lvreduce --resizefs` requiere desmontar para ext4?

### Ejercicio 3: Migrar y retirar un disco

Continuando del ejercicio anterior:

1. Verifica en qué PV reside cada LV: `pvdisplay -m /dev/vdb` y `pvdisplay -m /dev/vdc`
2. Añade un tercer disco: `pvcreate /dev/vdd && vgextend vg_lab /dev/vdd`
3. Migra todo de `/dev/vdb` a `/dev/vdd`: `pvmove /dev/vdb /dev/vdd`
4. Verifica: `pvdisplay -m /dev/vdb` — ¿tiene PEs asignados?
5. Retira `/dev/vdb` del VG: `vgreduce vg_lab /dev/vdb`
6. Limpia: `pvremove /dev/vdb`
7. Verifica que todo sigue montado y funcional: `findmnt /mnt/web`, `findmnt /mnt/db`
8. Limpieza final: desmonta todo, `vgremove -f vg_lab`, `pvremove /dev/vdc /dev/vdd`

> **Pregunta de reflexión**: `pvmove` mueve datos online sin desmontar. ¿Qué pasa con el rendimiento de I/O del filesystem mientras `pvmove` está en progreso? ¿Por qué?
