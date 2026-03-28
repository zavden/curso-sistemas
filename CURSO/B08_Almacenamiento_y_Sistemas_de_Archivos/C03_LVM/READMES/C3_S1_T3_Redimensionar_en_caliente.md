# Redimensionar en caliente

## Índice

1. [Qué significa "en caliente"](#qué-significa-en-caliente)
2. [Matriz de soporte: qué se puede hacer online](#matriz-de-soporte-qué-se-puede-hacer-online)
3. [Extender ext4 online](#extender-ext4-online)
4. [Extender XFS online](#extender-xfs-online)
5. [El atajo: lvextend --resizefs](#el-atajo-lvextend---resizefs)
6. [Escenario completo: disco nuevo → LV más grande](#escenario-completo-disco-nuevo--lv-más-grande)
7. [Reducir ext4 (requiere offline)](#reducir-ext4-requiere-offline)
8. [Reducir con XFS: la alternativa](#reducir-con-xfs-la-alternativa)
9. [Redimensionar rootfs en caliente](#redimensionar-rootfs-en-caliente)
10. [Monitorizar espacio y reaccionar](#monitorizar-espacio-y-reaccionar)
11. [Errores comunes](#errores-comunes)
12. [Cheatsheet](#cheatsheet)
13. [Ejercicios](#ejercicios)

---

## Qué significa "en caliente"

Redimensionar "en caliente" (online, live, hot) significa cambiar el tamaño de un LV y su filesystem **mientras está montado y en uso**, sin interrupción del servicio.

```
┌──────────────────────────────────────────────────────────┐
│                  Redimensionar en caliente                │
│                                                          │
│   Aplicación escribiendo en /mnt/data                    │
│        │                                                 │
│        ▼                                                 │
│   ┌─────────────┐     lvextend      ┌─────────────┐     │
│   │  Filesystem  │ ──────────────►  │  Filesystem  │     │
│   │  ext4 10 GiB │    + resize      │  ext4 15 GiB │     │
│   │  (montado)   │                  │  (montado)   │     │
│   └─────────────┘                   └─────────────┘     │
│                                                          │
│   Sin umount. Sin reinicio. Sin downtime.                │
└──────────────────────────────────────────────────────────┘
```

Esto es posible porque:
- El kernel permite cambiar el tamaño del dispositivo de bloque (LV) dinámicamente
- Los filesystems ext4 y XFS soportan crecimiento online
- LVM coordina el cambio con el Device Mapper

---

## Matriz de soporte: qué se puede hacer online

```
┌──────────────┬──────────────────────┬──────────────────────┐
│ Operación    │ ext4                 │ XFS                  │
├──────────────┼──────────────────────┼──────────────────────┤
│ Crecer LV    │ ✓ Online             │ ✓ Online             │
│ Crecer FS    │ ✓ Online (resize2fs) │ ✓ Online (xfs_growfs)│
│ Reducir LV   │ ✗ Offline            │ ✗ Offline            │
│ Reducir FS   │ ✗ Offline (resize2fs)│ ✗ IMPOSIBLE          │
│ --resizefs   │ ✓ (crecer online,    │ ✓ (solo crecer)      │
│              │    reducir offline)  │                      │
└──────────────┴──────────────────────┴──────────────────────┘

Online  = montado, en uso, sin interrupción
Offline = requiere umount (y fsck para ext4)
```

**Resumen**: crecer siempre funciona online. Reducir requiere offline para ext4 y es imposible para XFS.

---

## Extender ext4 online

Dos pasos: primero extender el LV, luego extender el filesystem.

### Paso 1: verificar espacio disponible

```bash
# ¿Cuánto espacio libre tiene el VG?
vgs
#   VG   #PV #LV #SN Attr   VSize  VFree
#   vg0    2   2   0 wz--n- 19.99g  5.99g   ← 5.99 GiB libres

# Tamaño actual del LV
lvs /dev/vg0/lv_data
#   LV      VG   Attr       LSize
#   lv_data vg0  -wi-ao---- 10.00g

# Espacio en el filesystem
df -h /mnt/data
#   Filesystem               Size  Used  Avail  Use%  Mounted on
#   /dev/mapper/vg0-lv_data  9.8G  5.2G  4.1G   56%   /mnt/data
```

### Paso 2: extender el LV

```bash
lvextend -L +5G /dev/vg0/lv_data
#   Size of logical volume vg0/lv_data changed from 10.00 GiB to 15.00 GiB.
#   Logical volume vg0/lv_data successfully resized.
```

En este punto, el LV tiene 15 GiB pero el filesystem sigue viendo 10 GiB:

```bash
lvs /dev/vg0/lv_data
#   LV      LSize
#   lv_data 15.00g        ← LV: 15 GiB

df -h /mnt/data
#   Size  9.8G            ← filesystem: todavía 10 GiB
```

### Paso 3: extender el filesystem

```bash
resize2fs /dev/vg0/lv_data
#   resize2fs 1.46.5 (30-Dec-2021)
#   Filesystem at /dev/vg0/lv_data is mounted on /mnt/data; on-line resizing required
#   old_desc_blocks = 2, new_desc_blocks = 2
#   The filesystem on /dev/vg0/lv_data is now 3932160 (4k) blocks long.
```

> **Predicción**: `resize2fs` sin argumento de tamaño crece hasta ocupar todo el LV. Detecta que el filesystem está montado y usa online resizing. El proceso es instantáneo para los metadatos; los nuevos bloques quedan disponibles inmediatamente.

### Paso 4: verificar

```bash
df -h /mnt/data
#   Filesystem               Size  Used  Avail  Use%  Mounted on
#   /dev/mapper/vg0-lv_data  15G   5.2G  8.8G   37%   /mnt/data
```

El filesystem ahora ve los 15 GiB completos. La aplicación que estaba escribiendo no se interrumpió.

---

## Extender XFS online

XFS usa `xfs_growfs` en lugar de `resize2fs`. La diferencia clave: **xfs_growfs recibe el punto de montaje, no el dispositivo**.

### Procedimiento

```bash
# 1. Extender el LV
lvextend -L +5G /dev/vg0/lv_logs

# 2. Extender el filesystem XFS
xfs_growfs /mnt/logs          # ← punto de montaje, NO dispositivo
#   meta-data=/dev/mapper/vg0-lv_logs  isize=512  agcount=4, agsize=655360 blks
#   data     =                         bsize=4096 blocks=2621440, imaxpct=25
#   ...
#   data blocks changed from 2621440 to 3932160

# 3. Verificar
df -h /mnt/logs
```

### Comparación de sintaxis

```
ext4:   resize2fs /dev/vg0/lv_data       ← dispositivo
XFS:    xfs_growfs /mnt/logs             ← punto de montaje

Ambos: online, sin desmontar, instantáneo
```

> **Error frecuente**: ejecutar `xfs_growfs /dev/vg0/lv_logs` — funciona en versiones recientes de xfs_growfs, pero la forma canónica y portable es usar el punto de montaje.

---

## El atajo: lvextend --resizefs

`--resizefs` combina `lvextend` + `resize2fs`/`xfs_growfs` en un solo comando. Es la forma **recomendada**.

```bash
# ext4 — un solo comando
lvextend -L +5G --resizefs /dev/vg0/lv_data
#   Size of logical volume vg0/lv_data changed from 10.00 GiB to 15.00 GiB.
#   Logical volume vg0/lv_data successfully resized.
#   resize2fs 1.46.5 (30-Dec-2021)
#   ...
#   The filesystem on /dev/vg0/lv_data is now 3932160 (4k) blocks long.

# XFS — mismo comando
lvextend -L +5G --resizefs /dev/vg0/lv_logs
#   ...
#   meta-data=/dev/mapper/vg0-lv_logs...
#   data blocks changed from 2621440 to 3932160
```

`--resizefs` detecta automáticamente el tipo de filesystem y ejecuta la herramienta correcta.

### Variantes

```bash
# Añadir tamaño
lvextend -L +5G --resizefs /dev/vg0/lv_data

# Tamaño absoluto
lvextend -L 20G --resizefs /dev/vg0/lv_data

# Todo el espacio libre
lvextend -l +100%FREE --resizefs /dev/vg0/lv_data

# Forma corta: -r es alias de --resizefs
lvextend -L +5G -r /dev/vg0/lv_data
```

> **¿Por qué no usar siempre --resizefs?** No hay razón para no usarlo. Es más seguro porque asegura que LV y filesystem quedan sincronizados. La única excepción es si el LV no tiene filesystem (swap, raw device).

---

## Escenario completo: disco nuevo → LV más grande

El escenario más común en producción: un LV se está quedando sin espacio y necesitas añadir un disco nuevo.

```
Estado inicial:
  VG: vg0 (1 disco, 50 GiB, 0 libre)
  LV: lv_data (50 GiB, 95% uso)
  Filesystem: ext4 montado en /mnt/data

Objetivo: añadir 50 GiB a /mnt/data sin downtime
```

### Paso a paso

```bash
# 1. Conectar nuevo disco a la VM (hot-plug)
#    En el host:
virsh attach-disk lab-vm1 /var/lib/libvirt/images/extra.qcow2 vdd \
    --driver qemu --subdriver qcow2 --persistent

# 2. En la VM: verificar que el disco apareció
lsblk
#   vdd    252:48  0  50G  0 disk          ← nuevo disco

# 3. Inicializar como PV
pvcreate /dev/vdd

# 4. Añadir al VG existente
vgextend vg0 /dev/vdd

# 5. Verificar espacio disponible
vgs
#   VG   VSize   VFree
#   vg0  99.99g  49.99g   ← ahora hay espacio

# 6. Extender LV + filesystem (online, sin desmontar)
lvextend -l +100%FREE --resizefs /dev/vg0/lv_data

# 7. Verificar
df -h /mnt/data
#   Size  98G  Used 46G  Avail 47G  Use% 48%
#   ← el filesystem creció de 50G a ~98G sin interrupción
```

```
Resultado:
┌──────────┐  ┌──────────┐
│ /dev/vdb │  │ /dev/vdd │    2 PVs
│  50 GiB  │  │  50 GiB  │
└────┬─────┘  └────┬─────┘
     └──────┬──────┘
            ▼
     ┌──────────────┐
     │  vg0 (100G)  │    1 VG
     └──────┬───────┘
            ▼
     ┌──────────────┐
     │  lv_data     │    1 LV (100 GiB)
     │  ext4        │    filesystem extendido online
     │  /mnt/data   │
     └──────────────┘
```

Todo ocurrió con la aplicación en ejecución, sin desmontar, sin reiniciar.

---

## Reducir ext4 (requiere offline)

Reducir un filesystem **siempre requiere desmontarlo** (offline). Además, ext4 requiere `e2fsck` antes de reducir.

### El orden es crítico

```
⚠ REDUCIR — Orden obligatorio:

  1. umount          desmontar
  2. e2fsck -f       verificar (obligatorio)
  3. resize2fs N     reducir filesystem al nuevo tamaño
  4. lvreduce -L N   reducir LV al mismo tamaño
  5. mount           remontar

  Si inviertes 3 y 4 → PÉRDIDA DE DATOS GARANTIZADA
```

```
  ┌─────────────────────────────────────────────────┐
  │  ¿Por qué ese orden?                            │
  │                                                 │
  │  El filesystem ocupa los bloques del LV:        │
  │                                                 │
  │  LV:  [████████████████████████████████]  20 GiB│
  │  FS:  [████████████████████████████████]  20 GiB│
  │                                                 │
  │  Si reduces el FS primero (a 10 GiB):           │
  │  LV:  [████████████████████████████████]  20 GiB│
  │  FS:  [████████████████]................  10 GiB│
  │        ← datos aquí →   ← vacío, seguro →      │
  │                                                 │
  │  Ahora puedes reducir el LV a 10 GiB sin riesgo│
  │  LV:  [████████████████]                  10 GiB│
  │  FS:  [████████████████]                  10 GiB│
  │                                                 │
  │  Pero si reduces el LV PRIMERO:                 │
  │  LV:  [████████████████]                  10 GiB│
  │  FS:  [████████████████████████████████]  20 GiB│
  │                       ↑ CORTADO ↑ = CORRUPCIÓN  │
  └─────────────────────────────────────────────────┘
```

### Procedimiento manual

```bash
# 1. Desmontar
umount /mnt/data

# 2. Verificar integridad (obligatorio antes de resize2fs en shrink)
e2fsck -f /dev/vg0/lv_data
# Pass 1-5... /dev/vg0/lv_data: 150/655360 files ...

# 3. Reducir filesystem a 10 GiB
resize2fs /dev/vg0/lv_data 10G
# Resizing the filesystem on /dev/vg0/lv_data to 2621440 (4k) blocks.
# The filesystem on /dev/vg0/lv_data is now 2621440 (4k) blocks long.

# 4. Reducir LV a 10 GiB
lvreduce -L 10G /dev/vg0/lv_data
# WARNING: Reducing active logical volume to 10.00 GiB.
# THIS MAY DESTROY YOUR DATA (filesystem etc.)
# Do you really want to reduce vg0/lv_data? [y/n]: y

# 5. Remontar
mount /dev/vg0/lv_data /mnt/data

# 6. Verificar
df -h /mnt/data
```

### Procedimiento con --resizefs (recomendado)

```bash
# Desmontar (sigue siendo necesario)
umount /mnt/data

# --resizefs maneja e2fsck + resize2fs + lvreduce en orden correcto
lvreduce -L 10G --resizefs /dev/vg0/lv_data
# fsck from util-linux 2.37.4
# /dev/mapper/vg0-lv_data: clean ...
# resize2fs ...
# Reducing logical volume ...

# Remontar
mount /dev/vg0/lv_data /mnt/data
```

> **Predicción**: `lvreduce --resizefs` ejecutará e2fsck, luego resize2fs al tamaño indicado, y finalmente lvreduce. Si hay datos que no caben en el nuevo tamaño, resize2fs fallará y el proceso se aborta sin daño.

---

## Reducir con XFS: la alternativa

XFS **no soporta reducción**. No existe forma de encoger un filesystem XFS. La única alternativa es crear uno nuevo más pequeño y migrar los datos.

### Procedimiento de migración

```bash
# Situación: lv_logs (XFS, 20 GiB) necesita ser 10 GiB

# 1. Crear un LV temporal con el tamaño deseado
lvcreate -L 10G -n lv_logs_new vg0

# 2. Formatear
mkfs.xfs /dev/vg0/lv_logs_new

# 3. Montar el nuevo
mkdir -p /mnt/logs_new
mount /dev/vg0/lv_logs_new /mnt/logs_new

# 4. Copiar datos (preservando permisos y atributos)
rsync -aAXv /mnt/logs/ /mnt/logs_new/

# 5. Desmontar ambos
umount /mnt/logs
umount /mnt/logs_new

# 6. Eliminar el viejo
lvremove -f /dev/vg0/lv_logs

# 7. Renombrar el nuevo
lvrename vg0 lv_logs_new lv_logs

# 8. Montar con el nombre original
mount /dev/vg0/lv_logs /mnt/logs

# 9. Actualizar fstab si usa el nombre del LV
#    (si usa UUID, el UUID cambió — actualizar)
blkid /dev/vg0/lv_logs
# Actualizar UUID en /etc/fstab
```

> **Consejo**: por esto es importante no asignar todo el espacio del VG al crear LVs. Necesitas espacio libre para crear el LV temporal de migración.

---

## Redimensionar rootfs en caliente

Extender el rootfs (`/`) online es uno de los escenarios más comunes y útiles de LVM.

### Extender / (online — sin desmontar)

```bash
# Verificar configuración actual
df -h /
#   Filesystem               Size  Used  Avail  Use%
#   /dev/mapper/vg0-lv_root  10G   8.5G  1.1G   89%

# Verificar espacio libre en VG
vgs
#   VG   VFree
#   vg0  10.00g   ← hay espacio

# Extender — funciona online incluso para rootfs
lvextend -L +5G --resizefs /dev/vg0/lv_root

# Verificar
df -h /
#   Filesystem               Size  Used  Avail  Use%
#   /dev/mapper/vg0-lv_root  15G   8.5G  5.6G   60%
```

No necesitas emergency mode, ni live CD, ni reinicio. El kernel permite extender el rootfs online.

### Extender / cuando el VG está lleno

Si el VG no tiene espacio libre, necesitas añadir un disco:

```bash
# 1. Añadir disco (hot-plug o reboot)
# 2. Inicializar y extender VG
pvcreate /dev/vdd
vgextend vg0 /dev/vdd

# 3. Extender rootfs
lvextend -l +100%FREE --resizefs /dev/vg0/lv_root
```

### ¿Se puede reducir rootfs?

Técnicamente sí para ext4, pero requiere arrancar desde un medio externo (live CD) porque no puedes desmontar `/` desde el sistema en ejecución. En la práctica, rara vez vale la pena.

---

## Monitorizar espacio y reaccionar

### Verificar uso actual

```bash
# Espacio en filesystems
df -h
#   Filesystem               Size  Used  Avail  Use%  Mounted on
#   /dev/mapper/vg0-lv_root  15G   8.5G  5.6G   60%  /
#   /dev/mapper/vg0-lv_data  50G   47G   1.5G   97%  /mnt/data  ← ¡casi lleno!

# Espacio libre en VG
vgs -o vg_name,vg_size,vg_free
#   VG   VSize   VFree
#   vg0  99.99g  20.00g   ← 20 GiB disponibles para crecer

# Espacio libre por PV
pvs
```

### Flujo de reacción ante disco lleno

```
  df -h muestra 95% uso en /mnt/data
          │
          ▼
  ¿Hay espacio libre en el VG?
  vgs → VFree
          │
     Sí   │   No
          │    │
          │    ▼
          │   ¿Puedo añadir un disco?
          │    │
          │   Sí → pvcreate + vgextend
          │    │
          │    No → liberar espacio en el FS
          │         o mover datos
          ▼
  lvextend -l +100%FREE --resizefs /dev/vg0/lv_data
          │
          ▼
  df -h → verificar
```

### Script de alerta simple

```bash
#!/bin/bash
# check-space.sh — alertar si uso > 90%

THRESHOLD=90

df -h --output=target,pcent | tail -n +2 | while read mount usage; do
    pct=${usage%\%}
    if [ "$pct" -gt "$THRESHOLD" ] 2>/dev/null; then
        echo "WARNING: $mount at ${usage} usage"
    fi
done
```

---

## Errores comunes

### 1. Extender el LV pero olvidar el filesystem

```bash
# ✗ Solo lvextend — el filesystem no creció
lvextend -L +5G /dev/vg0/lv_data
df -h /mnt/data    # sigue mostrando el tamaño viejo

# ✓ Siempre usar --resizefs o ejecutar resize2fs/xfs_growfs después
lvextend -L +5G --resizefs /dev/vg0/lv_data
```

### 2. Confundir resize2fs con xfs_growfs

```bash
# ✗ resize2fs en XFS
resize2fs /dev/vg0/lv_logs
# resize2fs: Bad magic number in super-block

# ✗ xfs_growfs en ext4
xfs_growfs /mnt/data
# xfs_growfs: /mnt/data is not a mounted XFS filesystem

# ✓ Verificar tipo y usar la herramienta correcta
blkid /dev/vg0/lv_data    # TYPE="ext4" → resize2fs
blkid /dev/vg0/lv_logs    # TYPE="xfs"  → xfs_growfs

# ✓✓ O usar --resizefs que elige automáticamente
lvextend -L +5G --resizefs /dev/vg0/lv_data
```

### 3. Intentar reducir XFS

```bash
# ✗ XFS no soporta shrink
lvreduce -L 10G --resizefs /dev/vg0/lv_logs
# fsadm: Xfs filesystem does not support shrinking

# ✓ Migrar a un LV nuevo (ver sección "Reducir con XFS")
```

### 4. No verificar espacio libre en el VG antes de extender

```bash
# ✗ Intentar extender sin espacio
lvextend -L +50G /dev/vg0/lv_data
# Insufficient free space in volume group

# ✓ Verificar primero
vgs    # ¿hay VFree suficiente?
# Si no: pvcreate + vgextend con un nuevo disco
```

### 5. Especificar tamaño incorrecto al reducir

```bash
# ✗ resize2fs con un tamaño más grande que el LV
#    (no debería pasar, pero verificar)

# ✗ resize2fs y lvreduce con tamaños distintos
resize2fs /dev/vg0/lv_data 10G
lvreduce -L 8G /dev/vg0/lv_data    # ¡2 GiB de filesystem cortado!

# ✓ Mismo tamaño en ambos, o usar --resizefs
resize2fs /dev/vg0/lv_data 10G
lvreduce -L 10G /dev/vg0/lv_data

# ✓✓ --resizefs sincroniza automáticamente
lvreduce -L 10G --resizefs /dev/vg0/lv_data
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│            Redimensionar en caliente — Referencia rápida         │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  CRECER (online — sin desmontar):                                │
│    lvextend -L +5G -r /dev/vg0/lv_data     ext4 o XFS           │
│    lvextend -l +100%FREE -r /dev/vg0/lv    todo el espacio       │
│                                                                  │
│  CRECER (manual, dos pasos):                                     │
│    lvextend -L +5G /dev/vg0/lv_data                              │
│    resize2fs /dev/vg0/lv_data              ext4 (dispositivo)    │
│    xfs_growfs /mnt/data                    XFS  (mountpoint)     │
│                                                                  │
│  REDUCIR ext4 (offline — requiere umount):                       │
│    umount /mnt/data                                              │
│    lvreduce -L 10G -r /dev/vg0/lv_data     automático            │
│    mount /dev/vg0/lv_data /mnt/data                              │
│                                                                  │
│  REDUCIR XFS: IMPOSIBLE                                          │
│    Alternativa: crear LV nuevo + rsync + renombrar               │
│                                                                  │
│  AÑADIR DISCO AL POOL:                                           │
│    pvcreate /dev/vdd                                             │
│    vgextend vg0 /dev/vdd                                         │
│    lvextend -l +100%FREE -r /dev/vg0/lv_data                     │
│                                                                  │
│  ROOTFS (/):                                                     │
│    Crecer: online, mismo procedimiento                           │
│    Reducir: requiere live CD (no se puede umount /)              │
│                                                                  │
│  SOPORTE:                                                        │
│    ┌────────┬──────────────┬──────────────┐                      │
│    │        │ Crecer       │ Reducir      │                      │
│    │ ext4   │ ✓ Online     │ ✗ Offline    │                      │
│    │ XFS    │ ✓ Online     │ ✗ Imposible  │                      │
│    └────────┴──────────────┴──────────────┘                      │
│                                                                  │
│  -r = --resizefs (SIEMPRE usarlo)                                │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Extender un LV con ext4 online

En tu VM de lab:

1. Crea el stack LVM:
   ```bash
   pvcreate /dev/vdb /dev/vdc
   vgcreate vg_lab /dev/vdb /dev/vdc
   lvcreate -L 500M -n lv_web vg_lab
   mkfs.ext4 /dev/vg_lab/lv_web
   mkdir -p /mnt/web
   mount /dev/vg_lab/lv_web /mnt/web
   ```
2. Escribe datos de prueba: `dd if=/dev/urandom of=/mnt/web/testfile bs=1M count=100`
3. Verifica el espacio: `df -h /mnt/web` y `vgs`
4. Extiende **sin desmontar**: `lvextend -L +300M --resizefs /dev/vg_lab/lv_web`
5. Verifica: `df -h /mnt/web` — ¿creció?
6. Verifica que los datos siguen intactos: `md5sum /mnt/web/testfile`
7. Repite el paso 4 pero **sin** `--resizefs`. Luego ejecuta `resize2fs` manualmente. Compara `lvs` con `df -h` antes y después del resize2fs

> **Pregunta de reflexión**: entre los pasos `lvextend` (sin --resizefs) y `resize2fs`, el LV es más grande que el filesystem. ¿Hay riesgo de pérdida de datos en ese estado intermedio? ¿Qué pasa si el sistema se reinicia en ese momento?

### Ejercicio 2: Reducir un LV con ext4

Continuando del ejercicio anterior:

1. Verifica el tamaño actual: `lvs /dev/vg_lab/lv_web` y `df -h /mnt/web`
2. Intenta reducir **sin desmontar**: `lvreduce -L 500M --resizefs /dev/vg_lab/lv_web` — ¿qué error ves?
3. Ahora hazlo correctamente:
   ```bash
   umount /mnt/web
   lvreduce -L 500M --resizefs /dev/vg_lab/lv_web
   mount /dev/vg_lab/lv_web /mnt/web
   ```
4. Verifica: `df -h /mnt/web` — ¿tiene 500 MiB?
5. ¿Los datos de prueba siguen ahí? `ls -lh /mnt/web/testfile`

> **Pregunta de reflexión**: ¿qué pasaría si intentas reducir el LV a 50 MiB pero el archivo de prueba ocupa 100 MiB? ¿En qué paso del proceso se detecta el error?

### Ejercicio 3: Escenario de producción — disco lleno

Simula un escenario de disco lleno y resuélvelo sin downtime:

1. Crea un LV pequeño:
   ```bash
   lvcreate -L 200M -n lv_app vg_lab
   mkfs.ext4 /dev/vg_lab/lv_app
   mkdir -p /mnt/app
   mount /dev/vg_lab/lv_app /mnt/app
   ```
2. Llénalo: `dd if=/dev/zero of=/mnt/app/fill bs=1M count=170`
3. Verifica: `df -h /mnt/app` — debería estar >90%
4. **Sin desmontar**, resuelve el problema:
   - Verifica espacio libre en VG: `vgs`
   - Extiende: `lvextend -L +300M --resizefs /dev/vg_lab/lv_app`
5. Verifica: `df -h /mnt/app`
6. Si el VG no tuviera espacio libre, ¿qué harías? Escribe los comandos necesarios sin ejecutarlos
7. Limpieza: desmonta todo, `vgremove -f vg_lab`, `pvremove /dev/vdb /dev/vdc`

> **Pregunta de reflexión**: en un servidor de producción con una base de datos corriendo, ¿es seguro ejecutar `lvextend --resizefs` sobre el LV donde reside la base de datos? ¿Qué precauciones tomarías?
