# XFS

## Índice

1. [Qué es XFS](#1-qué-es-xfs)
2. [Crear un filesystem XFS](#2-crear-un-filesystem-xfs)
3. [Estructura interna](#3-estructura-interna)
4. [xfs_admin: ajustar parámetros](#4-xfs_admin-ajustar-parámetros)
5. [xfs_info: inspeccionar el filesystem](#5-xfs_info-inspeccionar-el-filesystem)
6. [Redimensionar XFS](#6-redimensionar-xfs)
7. [xfs_repair: reparación](#7-xfs_repair-reparación)
8. [Diferencias clave con ext4](#8-diferencias-clave-con-ext4)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Qué es XFS

XFS es un filesystem de alto rendimiento creado por SGI en 1993 para estaciones
de trabajo IRIX. Fue portado a Linux en 2001. Es el **filesystem por defecto de
RHEL/AlmaLinux/Rocky/CentOS** desde RHEL 7.

```
┌────────────────────────────────────────────────────────────┐
│  Dónde encontrar XFS:                                      │
│                                                            │
│  RHEL 7+, AlmaLinux, Rocky, CentOS Stream  →  default /   │
│  SUSE Enterprise                           →  opción       │
│  Debian, Ubuntu                            →  disponible   │
│  Fedora                                    →  default /    │
│                                                            │
│  Si administras servidores RHEL, NECESITAS saber XFS.      │
└────────────────────────────────────────────────────────────┘
```

### Características principales

| Característica | Valor |
|----------------|-------|
| Tamaño máximo de archivo | 8 EiB |
| Tamaño máximo de filesystem | 8 EiB |
| Journaling | Sí (metadata journal) |
| Allocation groups | Sí (paralelismo de I/O) |
| Online resize | Solo crecer (no encoger) |
| Online defrag | Sí (`xfs_fsr`) |
| Reflink (copy-on-write) | Sí (desde XFS v5) |
| DAX (direct access) | Sí |

### Cuándo usar XFS

```
✓ Servidores RHEL/AlmaLinux (es el default)
✓ Archivos muy grandes (multimedia, bases de datos, VMs)
✓ Alto paralelismo de I/O (servidores de archivos)
✓ Particiones grandes (multi-terabyte)
✓ Examen RHCSA (obligatorio conocerlo)

✗ Necesitas encoger el filesystem → ext4
✗ Muchos archivos muy pequeños → ext4 (mejor)
✗ Necesitas snapshots integrados → Btrfs
```

---

## 2. Crear un filesystem XFS

### 2.1. Instalar xfsprogs

```bash
# Debian/Ubuntu (no instalado por defecto)
sudo apt install xfsprogs

# AlmaLinux/RHEL/Fedora (ya instalado)
sudo dnf install xfsprogs
```

### 2.2. mkfs.xfs básico

```bash
sudo mkfs.xfs /dev/vdb1
```

```
meta-data=/dev/vdb1              isize=512    agcount=4, agsize=65536 blks
         =                       sectsz=512   attr=2, projquota
data     =                       bsize=4096   blocks=262144, imaxpct=25
         =                       sunit=0      swidth=0 blks
naming   =version 2              bsize=4096   ascii-ci=0, ftype=1
log      =internal log           bsize=4096   blocks=16384, version=2
         =                       sectsz=512   sunit=0 blks, lazy-count=1
realtime =none                   extsz=4096   blocks=0, rtextents=0
```

La salida es más detallada que ext4 y muestra la estructura interna
inmediatamente.

### 2.3. Opciones de mkfs.xfs

```bash
# Con label
sudo mkfs.xfs -L "datos" /dev/vdb1

# Forzar (sobrescribir filesystem existente)
sudo mkfs.xfs -f /dev/vdb1

# Con label + forzar
sudo mkfs.xfs -f -L "datos" /dev/vdb1

# Tamaño de bloque (default 4096)
sudo mkfs.xfs -b size=4096 /dev/vdb1

# Número de allocation groups
sudo mkfs.xfs -d agcount=8 /dev/vdb1
```

| Opción | Significado | Default | Cuándo cambiar |
|--------|-------------|---------|----------------|
| `-L label` | Etiqueta | (ninguna) | Siempre — útil para identificar |
| `-f` | Forzar sobreescritura | No | Cuando ya hay un fs en el dispositivo |
| `-b size=N` | Tamaño de bloque | 4096 | Rara vez |
| `-d agcount=N` | Allocation groups | 4 | Discos grandes con mucho I/O paralelo |
| `-l size=N` | Tamaño del log (journal) | Automático | Workloads con muchos metadatos |

### 2.4. Sin bloques reservados

A diferencia de ext4, **XFS no reserva espacio para root**. Todo el espacio
está disponible para cualquier usuario:

```bash
# ext4: 5% reservado por defecto (tune2fs -m)
# XFS:  0% reservado — todo el disco es utilizable

# No hay equivalente de tune2fs -m en XFS
```

---

## 3. Estructura interna

### 3.1. Allocation Groups (AGs)

La estructura más importante de XFS. El filesystem se divide en **allocation
groups** independientes que permiten operaciones de I/O en paralelo:

```
┌──────────────────────────────────────────────────────────────┐
│                    FILESYSTEM XFS                            │
│                                                              │
│  ┌────────────┬────────────┬────────────┬────────────┐       │
│  │    AG 0    │    AG 1    │    AG 2    │    AG 3    │       │
│  │            │            │            │            │       │
│  │ Superblock │ AG Header  │ AG Header  │ AG Header  │       │
│  │ Free Space│ Free Space │ Free Space │ Free Space │       │
│  │ Inode B+  │ Inode B+   │ Inode B+   │ Inode B+   │       │
│  │ Data      │ Data       │ Data       │ Data       │       │
│  └────────────┴────────────┴────────────┴────────────┘       │
│                                                              │
│  Cada AG opera independientemente:                           │
│  • Su propio espacio libre (B+ tree)                         │
│  • Sus propios inodos (B+ tree)                              │
│  • Locks independientes → múltiples threads simultáneos      │
│                                                              │
│  ext4: un lock global → operaciones serializadas             │
│  XFS:  N locks (uno por AG) → paralelismo real               │
└──────────────────────────────────────────────────────────────┘
```

### 3.2. B+ trees en todas partes

XFS usa B+ trees para casi todas sus estructuras internas:

| Estructura | Tipo | Función |
|-----------|------|---------|
| Free space | B+ tree por tamaño y por offset | Encontrar bloques libres rápidamente |
| Inodos | B+ tree | Ubicar inodos eficientemente |
| Extents de archivos | B+ tree | Mapear datos de archivos grandes |
| Directorios | B+ tree (si son grandes) | Búsqueda rápida en directorios enormes |

Esto hace que XFS sea especialmente bueno con archivos y directorios enormes.

### 3.3. Journal (log)

XFS usa un journal solo para **metadatos** (no para datos). El journal puede
estar:

```
Log interno (default):
  Dentro del filesystem, en el AG 0

Log externo (opcional):
  En un dispositivo separado (mejor rendimiento)
  mkfs.xfs -l logdev=/dev/ssd_rapido /dev/disco_datos
```

---

## 4. xfs_admin: ajustar parámetros

`xfs_admin` es el equivalente de `tune2fs` para XFS. El filesystem debe estar
**desmontado**.

### 4.1. Ver label y UUID

```bash
sudo xfs_admin -l /dev/vdb1
# label = "datos"

sudo xfs_admin -u /dev/vdb1
# UUID = a1b2c3d4-e5f6-7890-abcd-ef1234567890
```

### 4.2. Cambiar label

```bash
sudo xfs_admin -L "nuevo-label" /dev/vdb1
# writing all SBs
# new label = "nuevo-label"
```

### 4.3. Cambiar UUID

```bash
# Generar nuevo UUID
sudo xfs_admin -U generate /dev/vdb1

# UUID específico
sudo xfs_admin -U "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" /dev/vdb1

# Limpiar UUID (nil)
sudo xfs_admin -U nil /dev/vdb1
```

### 4.4. Comparación xfs_admin vs tune2fs

| Acción | ext4 (tune2fs) | XFS (xfs_admin) |
|--------|----------------|-----------------|
| Ver label | `tune2fs -l` o `e2label` | `xfs_admin -l` |
| Cambiar label | `tune2fs -L X` o `e2label X` | `xfs_admin -L X` |
| Ver UUID | `tune2fs -l` o `blkid` | `xfs_admin -u` |
| Cambiar UUID | `tune2fs -U random` | `xfs_admin -U generate` |
| Bloques reservados | `tune2fs -m N` | No existe (0% siempre) |
| Check interval | `tune2fs -i 0` | No existe |
| Features | `tune2fs -O feature` | `xfs_admin -O feature` (limitado) |
| Debe desmontar | No (mayoría) | Sí (siempre) |

---

## 5. xfs_info: inspeccionar el filesystem

`xfs_info` muestra la información del filesystem. A diferencia de `xfs_admin`,
puede ejecutarse **con el filesystem montado**:

### 5.1. Uso básico

```bash
# Con mount point
sudo xfs_info /mnt

# Con dispositivo (debe estar montado)
sudo xfs_info /dev/vdb1
```

```
meta-data=/dev/vdb1              isize=512    agcount=4, agsize=65536 blks
         =                       sectsz=512   attr=2, projquota
data     =                       bsize=4096   blocks=262144, imaxpct=25
         =                       sunit=0      swidth=0 blks
naming   =version 2              bsize=4096   ascii-ci=0, ftype=1
log      =internal               bsize=4096   blocks=16384, version=2
         =                       sectsz=512   sunit=0 blks, lazy-count=1
realtime =none                   extsz=4096   blocks=0, rtextents=0
```

### 5.2. Campos clave

| Sección | Campo | Significado |
|---------|-------|-------------|
| meta-data | `isize=512` | Tamaño del inodo (512 bytes) |
| meta-data | `agcount=4` | Número de allocation groups |
| meta-data | `agsize=65536 blks` | Bloques por AG |
| data | `bsize=4096` | Tamaño de bloque |
| data | `blocks=262144` | Bloques totales (×4096 = 1 GiB) |
| data | `imaxpct=25` | Máximo 25% del espacio para inodos |
| log | `blocks=16384` | Tamaño del journal en bloques |
| log | `internal` | Journal dentro del filesystem |
| realtime | `none` | Sin sección realtime |

---

## 6. Redimensionar XFS

### 6.1. Crecer (online)

XFS puede crecer **mientras está montado**, usando `xfs_growfs`:

```bash
# 1. Ampliar la partición
sudo parted -s /dev/vdb resizepart 1 100%

# 2. Ampliar el filesystem (DEBE estar montado)
sudo xfs_growfs /mnt
```

**Diferencia clave con ext4**: `resize2fs` acepta el dispositivo (`/dev/vdb1`),
pero `xfs_growfs` requiere el **mount point** (`/mnt`).

```bash
# ✗ No funciona
sudo xfs_growfs /dev/vdb1
# Error: is not a mounted XFS filesystem

# ✓ Usar el mount point
sudo xfs_growfs /mnt
```

Salida:

```
meta-data=/dev/vdb1              isize=512    agcount=4, agsize=65536 blks
data     =                       bsize=4096   blocks=262144, imaxpct=25
data     =                       bsize=4096   blocks=524288, imaxpct=25
```

La línea `data` aparece dos veces: tamaño anterior y nuevo.

### 6.2. Encoger: NO es posible

```
┌────────────────────────────────────────────────────────────┐
│  ⚠ XFS NO SE PUEDE ENCOGER                                │
│                                                            │
│  No existe xfs_shrinkfs ni equivalente.                    │
│  Si necesitas reducir un XFS, la única opción es:          │
│                                                            │
│  1. Backup de los datos                                    │
│  2. Eliminar la partición                                  │
│  3. Crear una partición más pequeña                        │
│  4. mkfs.xfs en la nueva partición                         │
│  5. Restaurar los datos                                    │
│                                                            │
│  Si encoger es un requisito frecuente → usar ext4.         │
└────────────────────────────────────────────────────────────┘
```

---

## 7. xfs_repair: reparación

### 7.1. Uso básico

```bash
# El filesystem DEBE estar desmontado
sudo xfs_repair /dev/vdb1
```

```
Phase 1 - find and verify superblock...
Phase 2 - using internal log
Phase 3 - for each AG...
Phase 4 - check for duplicate blocks...
Phase 5 - rebuild AG headers and trees...
Phase 6 - check inode connectivity...
Phase 7 - verify and correct link counts...
done
```

### 7.2. Si el log está sucio

Si el filesystem no se desmontó limpiamente (crash, apagón), el log contiene
transacciones pendientes. `xfs_repair` se niega a reparar sin replaying el log:

```bash
sudo xfs_repair /dev/vdb1
# ERROR: The filesystem has a dirty log. Mount and unmount first.

# Solución: montar y desmontar para replay del log
sudo mount /dev/vdb1 /mnt
sudo umount /mnt

# Ahora sí:
sudo xfs_repair /dev/vdb1
```

Si no puedes montar (filesystem muy dañado):

```bash
# Forzar: descartar el log (pérdida de transacciones pendientes)
sudo xfs_repair -L /dev/vdb1
```

### 7.3. Dry run

```bash
# Solo verificar, sin reparar
sudo xfs_repair -n /dev/vdb1
```

### 7.4. xfs_repair vs fsck

```
ext4:   sudo fsck.ext4 /dev/vdb1     (o e2fsck)
XFS:    sudo xfs_repair /dev/vdb1    (NO usar fsck.xfs)
```

`fsck.xfs` existe pero es un **no-op** (no hace nada). Solo imprime un mensaje
sugiriendo usar `xfs_repair`. Esto es por diseño: XFS maneja la reparación de
forma diferente.

---

## 8. Diferencias clave con ext4

```
┌──────────────────────────────────────────────────────────────┐
│              ext4 vs XFS — COMPARACIÓN                       │
│                                                              │
│  Aspecto              ext4              XFS                  │
│  ─────────────        ─────────────     ─────────────        │
│  Default en           Debian, Ubuntu    RHEL, Alma, Fedora   │
│  Tamaño máx fs        1 EiB             8 EiB                │
│  Tamaño máx archivo   16 TiB            8 EiB                │
│  Espacio reservado    5% (configurable) 0%                   │
│  Encoger              Sí (offline)      NO                   │
│  Crecer online        Sí                Sí                   │
│  I/O paralelo         Limitado          Allocation groups    │
│  Archivos pequeños    Mejor             Peor                 │
│  Archivos grandes     Bueno             Mejor                │
│  Journal              Datos+metadatos   Solo metadatos       │
│  Inspeccionar         tune2fs -l        xfs_info             │
│  Modificar params     tune2fs           xfs_admin            │
│  Redimensionar        resize2fs DEV     xfs_growfs MOUNT     │
│  Reparar              e2fsck            xfs_repair           │
│  Crear                mkfs.ext4         mkfs.xfs             │
│  Defrag online        No                xfs_fsr              │
│  Reflink/COW          No                Sí (v5)              │
└──────────────────────────────────────────────────────────────┘
```

### Regla práctica

```
¿RHEL/AlmaLinux?        →  XFS (es el default, no luches contra él)
¿Debian/Ubuntu?         →  ext4 (es el default)
¿Necesitas encoger?     →  ext4 (XFS no puede)
¿Archivos enormes?      →  XFS (mejor rendimiento)
¿Examen RHCSA?          →  Dominar ambos
¿Labs de este curso?    →  Practicar ambos (por eso tenemos 2 VMs)
```

---

## 9. Errores comunes

### Error 1: usar fsck.xfs para reparar

```bash
# ✗ fsck.xfs no hace nada
sudo fsck.xfs /dev/vdb1
# "If you wish to check the consistency of an XFS filesystem...
#  please use xfs_repair."

# ✓ Usar xfs_repair
sudo xfs_repair /dev/vdb1
```

### Error 2: usar xfs_growfs con el dispositivo

```bash
# ✗ xfs_growfs no acepta el dispositivo
sudo xfs_growfs /dev/vdb1
# xfs_growfs: /dev/vdb1 is not a mounted XFS filesystem

# ✓ Usar el mount point
sudo xfs_growfs /mnt
```

### Error 3: intentar encoger XFS

```bash
# ✗ No existe forma de encoger XFS
sudo xfs_growfs -D 131072 /mnt    # tamaño menor → error
# data size 131072 too small, old size is 262144

# ✓ Si necesitas un XFS más pequeño:
# backup → mkfs.xfs en partición más pequeña → restore
```

### Error 4: olvidar `-f` al reformatear

```bash
# ✗ mkfs.xfs se niega a sobrescribir un filesystem existente
sudo mkfs.xfs /dev/vdb1
# mkfs.xfs: /dev/vdb1 appears to contain an existing filesystem (ext4)
# Use the -f option to force overwrite.

# ✓ Usar -f
sudo mkfs.xfs -f /dev/vdb1
```

A diferencia de `mkfs.ext4` que sobrescribe sin preguntar, `mkfs.xfs` protege
contra sobreescrituras accidentales. Esto es una buena feature de seguridad.

### Error 5: xfs_admin con el filesystem montado

```bash
# ✗ xfs_admin requiere filesystem desmontado
sudo xfs_admin -L "nuevo" /dev/vdb1
# xfs_admin: /dev/vdb1 contains a mounted filesystem

# ✓ Desmontar primero
sudo umount /mnt
sudo xfs_admin -L "nuevo" /dev/vdb1
```

---

## 10. Cheatsheet

```bash
# ── Crear filesystem ────────────────────────────────────────────
sudo mkfs.xfs /dev/vdb1                     # básico
sudo mkfs.xfs -L "label" /dev/vdb1          # con label
sudo mkfs.xfs -f /dev/vdb1                  # forzar sobreescritura
sudo mkfs.xfs -f -L "label" /dev/vdb1       # combinado

# ── Inspeccionar ────────────────────────────────────────────────
sudo xfs_info /mnt                           # info (montado)
sudo xfs_admin -l /dev/vdb1                  # ver label (desmontado)
sudo xfs_admin -u /dev/vdb1                  # ver UUID (desmontado)
sudo blkid /dev/vdb1                         # UUID y tipo

# ── Modificar parámetros (DESMONTADO) ───────────────────────────
sudo xfs_admin -L "nuevo-label" /dev/vdb1    # cambiar label
sudo xfs_admin -U generate /dev/vdb1         # generar nuevo UUID

# ── Redimensionar (MONTADO) ─────────────────────────────────────
# Solo crecer:
sudo xfs_growfs /mnt                         # al tamaño de la partición
sudo xfs_growfs -D 524288 /mnt              # a N bloques específicos
# Encoger: IMPOSIBLE

# ── Reparar (DESMONTADO) ────────────────────────────────────────
sudo xfs_repair /dev/vdb1                    # reparar
sudo xfs_repair -n /dev/vdb1                # dry run (solo verificar)
sudo xfs_repair -L /dev/vdb1                # forzar (descartar log)

# Si el log está sucio:
sudo mount /dev/vdb1 /mnt && sudo umount /mnt   # replay log
sudo xfs_repair /dev/vdb1                        # ahora sí

# ── Defragmentación (MONTADO) ───────────────────────────────────
sudo xfs_fsr /mnt                            # defragmentar
sudo xfs_fsr -v /mnt                         # verbose
sudo xfs_db -r -c frag /dev/vdb1            # ver fragmentación

# ── Diferencias con ext4 ────────────────────────────────────────
# ext4: tune2fs -l           XFS: xfs_info /mnt
# ext4: tune2fs -L X         XFS: xfs_admin -L X (desmontado)
# ext4: resize2fs /dev/vdb1  XFS: xfs_growfs /mnt (montado)
# ext4: e2fsck /dev/vdb1     XFS: xfs_repair /dev/vdb1
# ext4: mkfs.ext4            XFS: mkfs.xfs -f
```

---

## 11. Ejercicios

### Ejercicio 1: crear y explorar XFS

1. Crea una partición de 1G en `/dev/vdb`
2. Crea un filesystem XFS con label "lab-xfs": `sudo mkfs.xfs -L "lab-xfs" /dev/vdb1`
3. Monta en `/mnt` y ejecuta `xfs_info /mnt`:
   - ¿Cuántos allocation groups tiene?
   - ¿Cuál es el tamaño de bloque?
   - ¿Cuántos bloques tiene el log (journal)?
4. Ejecuta `df -h /mnt` — ¿cuánto espacio disponible tienes?
5. Compara con el ejercicio de ext4: ¿hay diferencia en espacio disponible?

**Pregunta de reflexión**: en ext4 de 1G, `df` mostraba ~950MB disponibles
(5% reservado + overhead). En XFS de 1G, ¿cuánto muestra? ¿Por qué XFS
muestra más espacio disponible?

### Ejercicio 2: comparar herramientas ext4 vs XFS

1. En `/dev/vdb`: crea ext4, monta, ejecuta `tune2fs -l`, desmonta
2. En `/dev/vdc`: crea XFS, monta, ejecuta `xfs_info`, desmonta
3. Cambia el label de ambos:
   - ext4: `sudo tune2fs -L "changed" /dev/vdb1` (¿necesitas desmontar?)
   - XFS: `sudo xfs_admin -L "changed" /dev/vdc1` (¿necesitas desmontar?)
4. Genera nuevo UUID en ambos:
   - ext4: `sudo tune2fs -U random /dev/vdb1`
   - XFS: `sudo xfs_admin -U generate /dev/vdc1`

**Pregunta de reflexión**: `tune2fs` puede cambiar el label con el filesystem
montado, pero `xfs_admin` requiere que esté desmontado. ¿Cuál enfoque es más
seguro? ¿Hay riesgo de corrupción al modificar metadatos en un filesystem
montado?

### Ejercicio 3: crecer XFS online

1. Crea una partición de 500M en `/dev/vdd` y formatea con XFS
2. Monta y verifica tamaño con `df -h`
3. **Sin desmontar**, amplía la partición a todo el disco:
   `sudo parted -s /dev/vdd resizepart 1 100%`
4. **Sin desmontar**, amplía el filesystem:
   `sudo xfs_growfs /mnt`
5. Verifica con `df -h` — debería mostrar ~2G
6. Intenta encoger: `sudo xfs_growfs -D 131072 /mnt` — observa el error

**Pregunta de reflexión**: tanto ext4 como XFS pueden crecer online. Pero ext4
usa `resize2fs /dev/vdd1` (dispositivo) y XFS usa `xfs_growfs /mnt` (mount
point). ¿Por qué la diferencia? Pista: piensa en que XFS necesita estar montado
para crecer, mientras que ext4 puede crecer montado O desmontado.
