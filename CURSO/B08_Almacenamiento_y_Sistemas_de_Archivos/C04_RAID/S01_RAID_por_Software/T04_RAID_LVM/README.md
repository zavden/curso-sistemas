# RAID + LVM

## Índice

1. [Por qué combinar RAID y LVM](#por-qué-combinar-raid-y-lvm)
2. [Orden de las capas](#orden-de-las-capas)
3. [Stack completo: disco → RAID → LVM → FS](#stack-completo-disco--raid--lvm--fs)
4. [Crear RAID + LVM paso a paso](#crear-raid--lvm-paso-a-paso)
5. [Extender el stack](#extender-el-stack)
6. [Gestionar fallos de disco](#gestionar-fallos-de-disco)
7. [¿LVM sobre RAID o RAID sobre LVM?](#lvm-sobre-raid-o-raid-sobre-lvm)
8. [Alternativa: LVM RAID nativo](#alternativa-lvm-raid-nativo)
9. [Escenario de producción: servidor con RAID + LVM](#escenario-de-producción-servidor-con-raid--lvm)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## Por qué combinar RAID y LVM

RAID y LVM resuelven problemas diferentes:

```
┌──────────────────────────────────────────────────────┐
│  RAID                        LVM                     │
│  ┌────────────────────┐      ┌────────────────────┐  │
│  │ Redundancia        │      │ Flexibilidad       │  │
│  │ Tolerancia a fallos│      │ Redimensionar      │  │
│  │ Rendimiento I/O    │      │ Snapshots          │  │
│  │                    │      │ Múltiples LVs      │  │
│  └────────────────────┘      └────────────────────┘  │
│                                                      │
│  Combinados:                                         │
│  ┌────────────────────────────────────────────────┐   │
│  │ Redundancia + Flexibilidad                    │   │
│  │ Discos protegidos contra fallo                │   │
│  │ Volúmenes que crecen/encogen según necesidad  │   │
│  │ Snapshots de volúmenes redundantes            │   │
│  └────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────┘
```

Sin combinarlos:
- RAID solo → un filesystem por array, tamaño fijo
- LVM solo → flexibilidad pero sin protección contra fallo de disco

Combinados → lo mejor de ambos mundos. Esta es la configuración estándar en servidores Linux de producción.

---

## Orden de las capas

El orden correcto es **RAID debajo, LVM encima**:

```
┌───────────────────────────────────────────────────┐
│                                                   │
│   Capa 5:  Filesystem (ext4, XFS)                 │
│              ▲                                    │
│   Capa 4:  Logical Volume (LV)                    │
│              ▲                                    │
│   Capa 3:  Volume Group (VG)                      │
│              ▲                                    │
│   Capa 2:  Physical Volume (PV)  ← sobre el RAID │
│              ▲                                    │
│   Capa 1:  RAID array (/dev/md0)                  │
│              ▲                                    │
│   Capa 0:  Discos físicos                         │
│                                                   │
└───────────────────────────────────────────────────┘
```

### ¿Por qué este orden?

```
Discos → md (RAID) → LVM → Filesystem

1. md combina discos en un dispositivo redundante (/dev/md0)
2. LVM trata /dev/md0 como un PV cualquiera
3. Del VG se crean LVs con la flexibilidad de LVM
4. Cada LV tiene su propio filesystem

Resultado: redundancia a nivel de disco + flexibilidad a nivel de volumen
```

El array RAID proporciona un "disco virtual" fiable. LVM trabaja encima de ese disco virtual sin saber ni importarle que debajo hay RAID. Cada capa se ocupa de lo suyo.

---

## Stack completo: disco → RAID → FS

Vista end-to-end con un ejemplo concreto:

```
┌─────────────────────────────────────────────────────────────┐
│                   STACK COMPLETO                             │
│                                                             │
│  DISCOS FÍSICOS                                             │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐                       │
│  │vdb   │ │vdc   │ │vdd   │ │vde   │   4 × 1 GiB          │
│  │1 GiB │ │1 GiB │ │1 GiB │ │1 GiB │                       │
│  └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘                       │
│     │        │        │        │                            │
│  RAID 5 (mdadm)                                             │
│  ┌──┴────────┴────────┴────────┴───┐                        │
│  │         /dev/md0                │   3 GiB útiles         │
│  │    RAID 5 (3 activos + 1 spare) │   (N-1) × 1 GiB       │
│  └──────────────┬──────────────────┘                        │
│                 │                                           │
│  LVM (pvcreate → vgcreate → lvcreate)                       │
│  ┌──────────────┴──────────────────┐                        │
│  │         PV: /dev/md0            │                        │
│  │         VG: vg_data             │   ~3 GiB               │
│  │  ┌─────────┐  ┌─────────┐      │                        │
│  │  │ lv_www  │  │ lv_db   │      │                        │
│  │  │ 1.5 GiB │  │ 1 GiB   │      │                        │
│  │  │  ext4   │  │  xfs    │      │                        │
│  │  │ /var/www│  │ /var/lib│      │                        │
│  │  └─────────┘  └─────────┘      │                        │
│  │  Libre: ~0.5 GiB               │                        │
│  └─────────────────────────────────┘                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Crear RAID + LVM paso a paso

### Paso 1: Crear el array RAID

```bash
# Crear RAID 5 con 3 discos + 1 spare
mdadm --create /dev/md0 --level=5 --raid-devices=3 \
    /dev/vdb /dev/vdc /dev/vdd --spare-devices=1 /dev/vde

# Esperar sincronización inicial
watch cat /proc/mdstat
# Esperar hasta [UUU]

# Guardar configuración
mdadm --detail --scan >> /etc/mdadm.conf
```

### Paso 2: Crear LVM sobre el RAID

```bash
# Inicializar el array como PV
pvcreate /dev/md0

# Crear VG
vgcreate vg_data /dev/md0

# Verificar
pvs
#   PV        VG      Fmt  Attr PSize PFree
#   /dev/md0  vg_data lvm2 a--  2.99g 2.99g

vgs
#   VG      #PV #LV Attr   VSize VFree
#   vg_data   1   0 wz--n- 2.99g 2.99g
```

### Paso 3: Crear Logical Volumes

```bash
# LV para web
lvcreate -L 1.5G -n lv_www vg_data

# LV para base de datos
lvcreate -L 1G -n lv_db vg_data

# Verificar
lvs
#   LV     VG      Attr       LSize
#   lv_db  vg_data -wi-a----- 1.00g
#   lv_www vg_data -wi-a----- 1.50g
```

### Paso 4: Crear filesystems y montar

```bash
# Filesystems
mkfs.ext4 /dev/vg_data/lv_www
mkfs.xfs /dev/vg_data/lv_db

# Puntos de montaje
mkdir -p /var/www /var/lib/dbdata

# Montar
mount /dev/vg_data/lv_www /var/www
mount /dev/vg_data/lv_db /var/lib/dbdata

# Persistencia en fstab
echo '/dev/mapper/vg_data-lv_www  /var/www         ext4  defaults  0  2' >> /etc/fstab
echo '/dev/mapper/vg_data-lv_db   /var/lib/dbdata  xfs   defaults  0  2' >> /etc/fstab
```

### Paso 5: Verificar el stack completo

```bash
lsblk
# NAME                 MAJ:MIN RM  SIZE RO TYPE   MOUNTPOINT
# vdb                  252:16   0    1G  0 disk
# └─md0                  9:0    0    2G  0 raid5
#   ├─vg_data-lv_www   253:0    0  1.5G  0 lvm    /var/www
#   └─vg_data-lv_db    253:1    0    1G  0 lvm    /var/lib/dbdata
# vdc                  252:32   0    1G  0 disk
# └─md0                  9:0    0    2G  0 raid5
# vdd                  252:48   0    1G  0 disk
# └─md0                  9:0    0    2G  0 raid5
# vde                  252:64   0    1G  0 disk
# └─md0                  9:0    0    2G  0 raid5
```

> **Predicción**: `lsblk` muestra la jerarquía completa: discos → md0 (raid5) → LVs. Los 4 discos aparecen como miembros de md0, y los LVs aparecen como hijos de md0 a través de Device Mapper.

---

## Extender el stack

### Añadir espacio al array RAID

Si necesitas más espacio, puedes añadir discos al array y luego extender LVM:

```bash
# 1. Añadir disco al array (crece el RAID)
mdadm --add /dev/md0 /dev/vdf
mdadm --grow /dev/md0 --raid-devices=4
# El array pasa de 3 a 4 discos activos
# reshape comienza (redistribución de datos + paridad)

# 2. Esperar reshape
watch cat /proc/mdstat
# Puede tardar bastante — el array sigue operativo

# 3. Extender el PV para ver el nuevo espacio
pvresize /dev/md0

# 4. Verificar
vgs
# VFree aumentó

# 5. Extender LVs según necesidad
lvextend -L +500M --resizefs /dev/vg_data/lv_www
```

### Añadir un segundo array RAID como PV

Alternativa: crear un segundo array y añadirlo al mismo VG.

```bash
# Crear segundo array con discos nuevos
mdadm --create /dev/md1 --level=1 --raid-devices=2 /dev/vdf /dev/vdg

# Añadir como PV al VG existente
pvcreate /dev/md1
vgextend vg_data /dev/md1

# Ahora el VG tiene espacio de ambos arrays
pvs
#   PV        VG      PSize
#   /dev/md0  vg_data 2.99g
#   /dev/md1  vg_data 1023.00m
```

> **Nota**: distintos niveles de RAID en el mismo VG son posibles pero no recomendados. Los LVs en un PV con RAID 1 tendrían distinta protección que los del PV con RAID 5. Mejor mantener un nivel consistente o separar VGs.

---

## Gestionar fallos de disco

Cuando un disco falla en el array RAID, la gestión ocurre **en la capa RAID**. LVM y los filesystems no se enteran — para ellos, `/dev/md0` sigue funcionando.

```
  Disco vdc falla
       │
       ▼
  RAID: md0 pasa a degradado [UU_]
  LVM:  vg_data sigue funcionando (no sabe de RAID)
  FS:   /var/www y /var/lib/dbdata siguen montados

  Acción: reemplazar disco en md0
  LVM y FS no requieren ninguna acción
```

### Procedimiento

```bash
# 1. Detectar fallo
cat /proc/mdstat
# md0 : active raid5 ... [3/2] [UU_]

# 2. Gestionar en la capa RAID (como en T02)
mdadm /dev/md0 --fail /dev/vdc
mdadm /dev/md0 --remove /dev/vdc
mdadm --zero-superblock /dev/vdc

# 3. Añadir reemplazo
mdadm /dev/md0 --add /dev/vdh

# 4. Monitorizar rebuild
watch cat /proc/mdstat

# 5. LVM y filesystems no necesitan cambios
# /var/www y /var/lib/dbdata siguieron operativos todo el tiempo
```

### Verificar que LVM sigue sano

```bash
# LVM ni se enteró del fallo
pvs
#   PV        VG      PSize
#   /dev/md0  vg_data 2.99g     ← sigue reportando normal

vgs
#   VG      Attr   VSize VFree
#   vg_data wz--n- 2.99g 0.49g  ← sin cambios

# Los LVs están intactos
lvs
df -h /var/www /var/lib/dbdata
```

---

## ¿LVM sobre RAID o RAID sobre LVM?

### Opción 1: LVM sobre RAID (md debajo, LVM encima) — RECOMENDADO

```
Discos → md (RAID) → PV → VG → LV → FS

Ventajas:
  ✓ Separación clara de responsabilidades
  ✓ RAID gestiona redundancia, LVM gestiona flexibilidad
  ✓ mdadm tiene herramientas maduras de monitorización
  ✓ Fácil de entender y diagnosticar
  ✓ Estándar de la industria

Desventajas:
  ✗ Dos herramientas que gestionar (mdadm + LVM)
  ✗ Todos los LVs comparten el mismo nivel de RAID
```

### Opción 2: RAID sobre LVM (LVM debajo, md encima) — NO RECOMENDADO

```
Discos → PV → VG → LV → md (RAID) → FS

  ✗ Complejo y contra-intuitivo
  ✗ Difícil de recuperar en caso de fallo
  ✗ No es una configuración estándar
  ✗ Poca documentación y soporte
```

### Opción 3: LVM RAID nativo (sin mdadm)

```
Discos → PV → VG → LV (con RAID integrado en LVM) → FS

  Ver siguiente sección
```

---

## Alternativa: LVM RAID nativo

LVM puede crear LVs con redundancia integrada usando Device Mapper RAID, sin necesidad de mdadm. Se usa el parámetro `--type raid1/raid5/raid6/raid10` en `lvcreate`.

### Crear LV con RAID integrado

```bash
# Inicializar PVs (un PV por disco, sin mdadm)
pvcreate /dev/vdb /dev/vdc /dev/vdd

# Crear VG con todos los PVs
vgcreate vg_data /dev/vdb /dev/vdc /dev/vdd

# Crear LV con RAID 5 integrado
lvcreate --type raid5 -L 1G -n lv_safe vg_data
# LVM gestiona la paridad y redundancia internamente

# Crear LV mirror (RAID 1)
lvcreate --type raid1 -m 1 -L 500M -n lv_mirror vg_data

# Verificar
lvs -o name,vg_name,lv_attr,lv_size,seg_type
#   LV        VG      Attr       LSize   Type
#   lv_mirror vg_data rwi-a-r--- 500.00m raid1
#   lv_safe   vg_data rwi-a-r--- 1.00g   raid5
```

### Monitorizar LVM RAID

```bash
# Estado de sincronización
lvs -o name,copy_percent,sync_percent
#   LV        Cpy%Sync
#   lv_safe   100.00        ← sincronizado

# Detalle de los "legs" (sub-LVs internos)
lvs -a -o name,seg_type,devices
#   lv_safe          raid5
#   [lv_safe_rimage_0]      /dev/vdb(0)
#   [lv_safe_rimage_1]      /dev/vdc(0)
#   [lv_safe_rimage_2]      /dev/vdd(0)
#   [lv_safe_rmeta_0]       /dev/vdb(64)
#   [lv_safe_rmeta_1]       /dev/vdc(64)
#   [lv_safe_rmeta_2]       /dev/vdd(64)
```

### Comparación mdadm vs LVM RAID

```
┌───────────────────────┬──────────────────┬──────────────────┐
│ Aspecto               │ mdadm + LVM      │ LVM RAID nativo  │
├───────────────────────┼──────────────────┼──────────────────┤
│ Herramientas          │ mdadm + lvm      │ Solo lvm         │
│ Madurez               │ Muy madura       │ Madura (RHEL 7+) │
│ Monitorización        │ mdstat, mdmonitor│ lvs, dmeventd    │
│ Flexibilidad por LV   │ No (todo el VG   │ Sí (cada LV pue- │
│                       │ mismo RAID)      │ de tener su nivel)│
│ Hot spare             │ Sí (nativo)      │ Limitado          │
│ Documentación         │ Extensa          │ Creciendo         │
│ Examen RHCSA          │ Sí (mdadm)       │ Probablemente no  │
│ Recomendación curso   │ Aprender ambos   │ Conocer que existe│
└───────────────────────┴──────────────────┴──────────────────┘
```

La ventaja principal de LVM RAID nativo es que **cada LV puede tener su propio nivel de RAID**:

```bash
# En el mismo VG:
lvcreate --type raid1 -m 1 -L 500M -n lv_critical vg_data   # RAID 1 para datos críticos
lvcreate --type raid5 -L 2G -n lv_general vg_data            # RAID 5 para datos generales
lvcreate -L 500M -n lv_temp vg_data                          # Sin RAID para temporales
```

Con mdadm + LVM, todos los LVs sobre el mismo array comparten el nivel de RAID.

> **Para este curso**: nos enfocamos en mdadm + LVM (el stack clásico) porque es el más documentado, el más probable en exámenes, y el más fácil de diagnosticar.

---

## Escenario de producción: servidor con RAID + LVM

### Diseño típico

```
┌─────────────────────────────────────────────────────────────┐
│              Servidor Linux de producción                    │
│                                                             │
│  BOOT (sin RAID o RAID 1 simple):                           │
│  ┌──────┐ ┌──────┐                                         │
│  │ sda1 │ │ sdb1 │   RAID 1 → /dev/md0 → /boot             │
│  │500MiB│ │500MiB│   (o partición simple si UEFI)           │
│  └──────┘ └──────┘                                         │
│                                                             │
│  SISTEMA (RAID 1 + LVM):                                    │
│  ┌──────┐ ┌──────┐                                         │
│  │ sda2 │ │ sdb2 │   RAID 1 → /dev/md1                     │
│  │ 50G  │ │ 50G  │       │                                  │
│  └──────┘ └──────┘       ▼                                  │
│                      PV /dev/md1                            │
│                      VG: vg_sys                             │
│                      ├── lv_root  20G  xfs   /              │
│                      ├── lv_var   15G  xfs   /var           │
│                      └── lv_swap   4G  swap                 │
│                                                             │
│  DATOS (RAID 5/6 + LVM):                                    │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐                       │
│  │ sdc  │ │ sdd  │ │ sde  │ │ sdf  │  RAID 5 → /dev/md2    │
│  │ 2T   │ │ 2T   │ │ 2T   │ │spare │      │                │
│  └──────┘ └──────┘ └──────┘ └──────┘      ▼                │
│                                       PV /dev/md2           │
│                                       VG: vg_data           │
│                                       ├── lv_www  1T  xfs   │
│                                       ├── lv_db   2T  xfs   │
│                                       └── libre   1T        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Orden de destrucción (limpieza total)

Si necesitas desmontar todo el stack, el orden es **inverso al de creación**:

```bash
# 1. Desmontar filesystems
umount /var/www
umount /var/lib/dbdata

# 2. Eliminar LVs (o el VG completo)
vgremove -f vg_data

# 3. Eliminar PV
pvremove /dev/md0

# 4. Detener array RAID
mdadm --stop /dev/md0

# 5. Limpiar superbloques
mdadm --zero-superblock /dev/vdb /dev/vdc /dev/vdd /dev/vde

# 6. Limpiar mdadm.conf y fstab
# Editar y eliminar las entradas correspondientes
```

---

## Errores comunes

### 1. Crear PV en discos individuales en lugar del array

```bash
# ✗ PV en cada disco — sin redundancia RAID
pvcreate /dev/vdb /dev/vdc /dev/vdd
vgcreate vg_data /dev/vdb /dev/vdc /dev/vdd
# Esto es LVM sin RAID — un disco falla y pierdes datos de ese PV

# ✓ Primero RAID, luego PV en el array
mdadm --create /dev/md0 --level=5 --raid-devices=3 /dev/vdb /dev/vdc /dev/vdd
pvcreate /dev/md0
vgcreate vg_data /dev/md0
```

### 2. Olvidar pvresize después de crecer el array

```bash
# ✗ Crecer array pero LVM no ve el espacio nuevo
mdadm --grow /dev/md0 --raid-devices=4
# VG sigue mostrando el tamaño viejo

# ✓ Notificar a LVM del cambio de tamaño
mdadm --grow /dev/md0 --raid-devices=4
# Esperar reshape...
pvresize /dev/md0
# Ahora vgs muestra el nuevo VFree
```

### 3. Intentar gestionar fallo de disco con LVM

```bash
# ✗ Pensar que el fallo se gestiona con LVM
pvs    # "Pero el PV está bien..."
# LVM no sabe que hay RAID debajo — no puede ver discos individuales

# ✓ El fallo se gestiona en la capa RAID
cat /proc/mdstat          # ver estado del array
mdadm --detail /dev/md0   # identificar disco fallido
mdadm /dev/md0 --fail ... # gestionar el fallo
# LVM no necesita intervención
```

### 4. Destruir las capas en orden incorrecto

```bash
# ✗ Detener RAID con LVM activo encima
mdadm --stop /dev/md0
# mdadm: fail to stop array /dev/md0: Device or resource busy
# (LVM tiene el PV abierto)

# ✓ Orden inverso: FS → LVM → RAID
umount /var/www
vgremove -f vg_data    # o lvremove + vgremove + pvremove
mdadm --stop /dev/md0
mdadm --zero-superblock /dev/vdb /dev/vdc /dev/vdd
```

### 5. No guardar ambas configuraciones

```bash
# ✗ Guardar mdadm.conf pero olvidar fstab (o viceversa)
# → RAID se ensambla pero filesystems no se montan

# ✓ Guardar todo
mdadm --detail --scan > /etc/mdadm.conf      # RAID
echo '/dev/mapper/vg_data-lv_www ...' >> /etc/fstab  # montaje
dracut --force                                 # initramfs
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│                  RAID + LVM — Referencia rápida                  │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ORDEN DE CAPAS (abajo → arriba):                                │
│    Discos → md (RAID) → PV → VG → LV → Filesystem               │
│                                                                  │
│  CREAR:                                                          │
│    mdadm --create /dev/md0 --level=5 --raid-devices=3 \          │
│        /dev/vdb /dev/vdc /dev/vdd                                │
│    pvcreate /dev/md0                                             │
│    vgcreate vg_data /dev/md0                                     │
│    lvcreate -L 10G -n lv_www vg_data                             │
│    mkfs.ext4 /dev/vg_data/lv_www                                 │
│    mount /dev/vg_data/lv_www /var/www                            │
│                                                                  │
│  EXTENDER:                                                       │
│    mdadm --add + --grow       crecer el array                    │
│    pvresize /dev/md0          notificar a LVM                    │
│    lvextend -r /dev/vg/lv     crecer LV + FS                    │
│                                                                  │
│  FALLO DE DISCO:                                                 │
│    Se gestiona en la capa RAID (mdadm)                           │
│    LVM y filesystems no requieren acción                         │
│                                                                  │
│  DESTRUIR (orden inverso):                                       │
│    umount → lvremove/vgremove → pvremove →                       │
│    mdadm --stop → --zero-superblock                              │
│                                                                  │
│  PERSISTENCIA (guardar las 3):                                   │
│    mdadm --detail --scan > /etc/mdadm.conf                       │
│    /etc/fstab (UUIDs de los LVs o paths /dev/mapper/)            │
│    dracut --force (initramfs)                                    │
│                                                                  │
│  LVM RAID NATIVO (alternativa sin mdadm):                        │
│    lvcreate --type raid5 -L 10G -n lv vg                         │
│    lvcreate --type raid1 -m 1 -L 5G -n lv vg                    │
│    Ventaja: distintos niveles por LV                             │
│    Desventaja: menos herramientas de monitorización              │
│                                                                  │
│  VERIFICAR STACK:                                                │
│    lsblk              jerarquía visual completa                  │
│    cat /proc/mdstat    estado RAID                                │
│    pvs / vgs / lvs     estado LVM                                │
│    df -h               espacio en filesystems                    │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Construir el stack completo

En tu VM de lab con 4 discos (`/dev/vdb`, `/dev/vdc`, `/dev/vdd`, `/dev/vde` de 1 GiB):

1. Crea un array RAID 5 con 3 discos + 1 spare:
   ```bash
   mdadm --create /dev/md0 --level=5 --raid-devices=3 \
       /dev/vdb /dev/vdc /dev/vdd --spare-devices=1 /dev/vde
   ```
2. Espera la sincronización: `watch cat /proc/mdstat`
3. Crea LVM sobre el array:
   ```bash
   pvcreate /dev/md0
   vgcreate vg_lab /dev/md0
   lvcreate -L 500M -n lv_web vg_lab
   lvcreate -L 500M -n lv_db vg_lab
   ```
4. Crea filesystems: ext4 en `lv_web`, XFS en `lv_db`
5. Monta en `/mnt/web` y `/mnt/db`
6. Escribe datos en ambos
7. Ejecuta `lsblk` — dibuja la jerarquía que ves
8. Verifica cada capa: `cat /proc/mdstat`, `pvs`, `vgs`, `lvs`, `df -h`

> **Pregunta de reflexión**: en `lsblk`, los 4 discos aparecen como padres de md0, y los LVs como hijos de md0. ¿Qué capa intermedia (LVM) es invisible en `lsblk`? ¿Cómo la verificarías?

### Ejercicio 2: Fallo de disco con stack completo

Continuando del ejercicio anterior:

1. Simula fallo de un disco: `mdadm /dev/md0 --fail /dev/vdc`
2. Verifica **cada capa**:
   - RAID: `cat /proc/mdstat` — ¿degradado? ¿el spare se activó?
   - LVM: `pvs` — ¿cambió algo?
   - FS: `df -h /mnt/web /mnt/db` — ¿siguen accesibles?
   - Datos: `cat /mnt/web/testfile` — ¿intactos?
3. Monitoriza el rebuild con el spare: `watch cat /proc/mdstat`
4. Cuando termine el rebuild:
   - ¿Cuántos discos activos y spares reporta `mdadm --detail /dev/md0`?
   - ¿LVM y filesystems necesitaron alguna acción?
5. Retira el disco fallido: `mdadm /dev/md0 --remove /dev/vdc`
6. Añádelo de nuevo como spare: `mdadm --zero-superblock /dev/vdc && mdadm /dev/md0 --add /dev/vdc`

> **Pregunta de reflexión**: el fallo de disco fue completamente transparente para LVM y los filesystems. ¿En qué escenario un fallo de disco en el RAID SÍ afectaría a LVM?

### Ejercicio 3: Extender y limpiar

Continuando:

1. Verifica espacio libre: `vgs`
2. Extiende `lv_web` con todo el espacio libre: `lvextend -l +100%FREE --resizefs /dev/vg_lab/lv_web`
3. Verifica: `df -h /mnt/web`
4. Ahora limpia todo el stack en el **orden correcto**:
   ```bash
   # Documenta cada paso y por qué va en ese orden
   umount /mnt/web /mnt/db
   vgremove -f vg_lab
   pvremove /dev/md0
   mdadm --stop /dev/md0
   mdadm --zero-superblock /dev/vdb /dev/vdc /dev/vdd /dev/vde
   ```
5. Verifica que no queda nada: `cat /proc/mdstat`, `pvs`, `lsblk`

> **Pregunta de reflexión**: ¿qué pasaría si intentas ejecutar `mdadm --stop /dev/md0` antes de `vgremove`? ¿Por qué el orden de destrucción es el inverso del de creación?
