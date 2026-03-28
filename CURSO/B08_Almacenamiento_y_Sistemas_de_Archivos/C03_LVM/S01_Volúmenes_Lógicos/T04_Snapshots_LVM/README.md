# Snapshots LVM

## Índice

1. [Qué es un snapshot LVM](#qué-es-un-snapshot-lvm)
2. [Copy-on-Write (CoW)](#copy-on-write-cow)
3. [Crear un snapshot](#crear-un-snapshot)
4. [Montar un snapshot](#montar-un-snapshot)
5. [Usar snapshots para backups](#usar-snapshots-para-backups)
6. [Revertir a un snapshot (merge)](#revertir-a-un-snapshot-merge)
7. [Eliminar un snapshot](#eliminar-un-snapshot)
8. [Monitorizar uso del snapshot](#monitorizar-uso-del-snapshot)
9. [Thin snapshots](#thin-snapshots)
10. [Limitaciones de los snapshots LVM](#limitaciones-de-los-snapshots-lvm)
11. [Errores comunes](#errores-comunes)
12. [Cheatsheet](#cheatsheet)
13. [Ejercicios](#ejercicios)

---

## Qué es un snapshot LVM

Un snapshot LVM es una **fotografía del estado de un LV en un instante dado**. Después de crear el snapshot, el LV original sigue funcionando normalmente, y el snapshot preserva el estado del momento en que fue creado.

```
Tiempo ──────────────────────────────────────────────────►

          t0: crear snapshot        t1: cambios en origen
          │                         │
          ▼                         ▼
  LV:     [██████████████]  ──►  [██████████████████]
          │ datos A B C D │       │ datos A X C Y Z  │
                                       ↑ modificados

  Snap:   [██████████████]  ──►  [██████████████]
          │ datos A B C D │       │ datos A B C D │
          │ (referencia)  │       │ (preservados) │
```

Usos principales:

- **Backup consistente**: snapshot congelado mientras el LV sigue en uso
- **Pruebas destructivas**: probar una actualización, revertir si falla
- **Punto de restauración**: antes de modificar configuración o datos críticos

---

## Copy-on-Write (CoW)

Los snapshots LVM no copian todos los datos al crearse. Usan **Copy-on-Write**: solo se copian los bloques que van a ser modificados en el LV original.

```
┌───────────────────────────────────────────────────────────────┐
│                    Copy-on-Write                               │
│                                                               │
│  CREAR SNAPSHOT (instantáneo, no copia datos):                │
│                                                               │
│  LV origen:  [A][B][C][D][E][F]                               │
│  Snapshot:   (vacío — solo punteros al origen)                 │
│                                                               │
│  ESCRIBIR en bloque C del origen:                              │
│                                                               │
│  1. Copiar C original → área del snapshot                      │
│  2. Escribir C' en el origen                                   │
│                                                               │
│  LV origen:  [A][B][C'][D][E][F]   ← C cambió a C'           │
│  Snapshot:        [C]               ← C original guardado     │
│                                                               │
│  LEER desde el snapshot:                                       │
│  - Bloque C → leer del área del snapshot (C original)          │
│  - Bloques A,B,D,E,F → leer del origen (no cambiaron)         │
│                                                               │
│  RESULTADO: snapshot ve [A][B][C][D][E][F] = estado original   │
└───────────────────────────────────────────────────────────────┘
```

Consecuencias del CoW:

- **Crear el snapshot es instantáneo** — no importa el tamaño del LV
- **El snapshot crece** conforme se modifican datos en el origen
- Si el snapshot se **llena** (se modificaron más bloques de los que caben), se **invalida**
- **Impacto en rendimiento**: cada escritura en el origen implica una lectura + escritura extra (copiar el bloque al snapshot antes de sobreescribirlo)

---

## Crear un snapshot

### Sintaxis

```bash
lvcreate --snapshot -L <tamaño> -n <nombre> <lv_origen>
```

El tamaño del snapshot define cuánto espacio hay para almacenar los bloques originales que cambien. No necesita ser tan grande como el LV original — solo debe cubrir el volumen de cambios esperado.

### Ejemplo básico

```bash
# LV original: 10 GiB con ext4 montado en /mnt/data
lvs /dev/vg0/lv_data
#   LV      VG   Attr       LSize
#   lv_data vg0  -wi-ao---- 10.00g

# Crear snapshot de 2 GiB (20% del origen — suficiente para cambios moderados)
lvcreate --snapshot -L 2G -n snap_data /dev/vg0/lv_data
#   Logical volume "snap_data" created.

# Verificar
lvs
#   LV        VG   Attr       LSize  Pool Origin  Data%
#   lv_data   vg0  owi-aos--- 10.00g
#   snap_data vg0  swi-a-s---  2.00g      lv_data 0.00
```

> **Predicción**: el snapshot aparece en `lvs` con atributos `s` (snapshot) y muestra el campo `Origin` apuntando al LV original. `Data%` indica cuánto del espacio del snapshot se ha usado (0% justo después de crearlo).

### Atributos en lvs

```
Origen:   owi-aos---
          │││ │││
          ││  ││└─ snapshot origin
          ││  │└── active
          ││  └─── open (montado)
          │└────── write
          └─────── origin (tiene snapshot)

Snapshot: swi-a-s---
          │││ │ │
          ││  │ └─ snapshot
          ││  └─── active
          │└────── write
          └─────── snapshot volume
```

### Tamaño del snapshot — ¿cuánto asignar?

| Escenario | Tamaño recomendado | Justificación |
|-----------|-------------------|---------------|
| Backup rápido (minutos) | 5-10% del origen | Pocos cambios durante el backup |
| Prueba de actualización (horas) | 20-30% del origen | Cambios moderados |
| Punto de restauración (días) | 50-100% del origen | Muchos cambios posibles |

Si no estás seguro, asigna más. Un snapshot que se llena se invalida y pierde toda utilidad.

---

## Montar un snapshot

Un snapshot se puede montar como cualquier otro dispositivo de bloque. Esto permite **acceder al estado congelado** del filesystem mientras el original sigue en uso.

### Montar snapshot ext4

```bash
# Crear punto de montaje
mkdir -p /mnt/snap_data

# Montar el snapshot
mount -o ro /dev/vg0/snap_data /mnt/snap_data

# Acceder a los datos del momento del snapshot
ls /mnt/snap_data/
```

> **Nota sobre ext4**: al montar un snapshot de un filesystem ext4 que estaba montado, el kernel puede querer replayar el journal. Usa `-o ro,nouuid` si recibes errores. En XFS, `-o ro,nouuid` es **obligatorio**.

### Montar snapshot XFS

XFS asigna un UUID al filesystem. Como el snapshot tiene el mismo UUID que el origen, no se puede montar sin la opción `nouuid`:

```bash
# ✗ Sin nouuid — falla
mount /dev/vg0/snap_data /mnt/snap_data
# mount: wrong fs type, bad option, bad superblock...
# (o: duplicate UUID)

# ✓ Con nouuid
mount -o ro,nouuid /dev/vg0/snap_data /mnt/snap_data
```

### Montar como lectura-escritura

Es posible montar un snapshot en modo escritura. Los cambios se escriben en el espacio del snapshot sin afectar al origen. Útil para probar modificaciones.

```bash
# Montar en rw (ext4)
mount /dev/vg0/snap_data /mnt/snap_data

# Hacer cambios — solo afectan al snapshot
echo "test" > /mnt/snap_data/newfile

# El origen no ve los cambios
ls /mnt/data/newfile
# ls: cannot access '/mnt/data/newfile': No such file or directory
```

---

## Usar snapshots para backups

El caso de uso más práctico: crear un snapshot, hacer backup desde el snapshot (estado consistente), eliminar el snapshot.

### Flujo

```
┌────────────────────────────────────────────────────────┐
│               Backup con snapshot                       │
│                                                        │
│  1. Crear snapshot                                      │
│     lvcreate --snapshot -L 2G -n snap /dev/vg0/lv_data  │
│                                                        │
│  2. Montar snapshot (ro)                                │
│     mount -o ro /dev/vg0/snap /mnt/snap                 │
│                                                        │
│  3. Hacer backup desde snapshot                         │
│     tar czf /backup/data.tar.gz -C /mnt/snap .          │
│     (o rsync, borgbackup, etc.)                         │
│                                                        │
│  4. Desmontar y eliminar snapshot                       │
│     umount /mnt/snap                                    │
│     lvremove -f /dev/vg0/snap                           │
│                                                        │
│  El LV original nunca se desmontó ni se detuvo          │
└────────────────────────────────────────────────────────┘
```

### Ejemplo completo

```bash
# Crear snapshot
lvcreate --snapshot -L 1G -n snap_data /dev/vg0/lv_data

# Montar como solo lectura
mkdir -p /mnt/snap_data
mount -o ro /dev/vg0/snap_data /mnt/snap_data

# Backup con tar
tar czf /backup/data-$(date +%Y%m%d).tar.gz -C /mnt/snap_data .

# Limpiar
umount /mnt/snap_data
lvremove -f /dev/vg0/snap_data
```

### ¿Por qué no hacer backup directamente del LV montado?

Porque mientras el backup corre, la aplicación sigue escribiendo. El archivo de backup contendría una mezcla de estados:

```
Sin snapshot (backup inconsistente):
  t0: tar lee archivo A (versión 1)
  t1: aplicación modifica A (versión 2) y B (versión 2)
  t2: tar lee archivo B (versión 2)
  → backup tiene A.v1 + B.v2 = estado que nunca existió

Con snapshot (backup consistente):
  t0: snapshot congela A.v1 + B.v1
  t1: tar lee A.v1 del snapshot
  t2: aplicación modifica A y B en el origen (no afecta snapshot)
  t3: tar lee B.v1 del snapshot
  → backup tiene A.v1 + B.v1 = estado real del momento t0
```

---

## Revertir a un snapshot (merge)

`lvconvert --merge` restaura el LV original al estado del snapshot. El snapshot se consume en el proceso (desaparece).

### Procedimiento

```bash
# 1. Desmontar el LV original
umount /mnt/data

# 2. Desmontar el snapshot (si estaba montado)
umount /mnt/snap_data

# 3. Merge — revertir el origen al estado del snapshot
lvconvert --merge /dev/vg0/snap_data
#   Merging of volume vg0/snap_data started.
#   vg0/lv_data: Merged: 100.00%

# 4. Remontar el LV original — ahora tiene el estado del snapshot
mount /dev/vg0/lv_data /mnt/data
```

> **Predicción**: después del merge, el LV `lv_data` contiene los datos tal como estaban cuando se creó el snapshot. El snapshot `snap_data` **desaparece** — fue absorbido por el merge.

### Merge de rootfs o LV en uso

Si el LV está en uso y no se puede desmontar (como rootfs), el merge se programa para el **próximo arranque**:

```bash
# Intentar merge del rootfs
lvconvert --merge /dev/vg0/snap_root
#   Merging of volume vg0/snap_root started.
#   vg0/lv_root: Merged: 0.00%
#   Merge will be completed on next activation of vg0/lv_root.

# Reiniciar para completar el merge
reboot
# Durante el arranque, LVM ejecuta el merge antes de montar
```

### Flujo: prueba destructiva con rollback

```
┌─────────────────────────────────────────────────────────┐
│          Prueba con posibilidad de rollback              │
│                                                         │
│  1. Crear snapshot (punto de restauración)               │
│     lvcreate --snapshot -L 5G -n snap /dev/vg0/lv_data   │
│                                                         │
│  2. Hacer cambios en el LV original                      │
│     (actualizar paquetes, migrar base de datos, etc.)    │
│                                                         │
│  3a. ¿Todo bien?                                         │
│      → Eliminar snapshot: lvremove -f /dev/vg0/snap      │
│                                                         │
│  3b. ¿Algo falló?                                        │
│      → Revertir: umount + lvconvert --merge              │
│      → El sistema vuelve al estado pre-cambios           │
└─────────────────────────────────────────────────────────┘
```

---

## Eliminar un snapshot

Un snapshot consume espacio del VG y añade overhead de rendimiento. Cuando ya no lo necesitas, elimínalo.

```bash
# Si está montado, desmontar primero
umount /mnt/snap_data

# Eliminar
lvremove /dev/vg0/snap_data
# Do you really want to remove active logical volume snap_data? [y/n]: y
#   Logical volume "snap_data" successfully removed.

# Sin confirmación
lvremove -f /dev/vg0/snap_data
```

Después de eliminar el snapshot:
- El espacio del snapshot vuelve al VG como espacio libre
- El overhead de CoW desaparece — rendimiento del LV original se restaura
- El LV original pierde el atributo `o` (origin) en `lvs`

---

## Monitorizar uso del snapshot

Un snapshot que se llena al 100% se **invalida automáticamente** y se vuelve inservible. Es crítico monitorizar su uso.

### Ver uso con lvs

```bash
lvs
#   LV        VG   Attr       LSize  Pool Origin  Data%
#   lv_data   vg0  owi-aos--- 10.00g
#   snap_data vg0  swi-a-s---  2.00g      lv_data 35.42

# Data% = 35.42% del snapshot usado
# A medida que el origen recibe escrituras, Data% crece
```

### Snapshot invalidado

```bash
lvs
#   LV        VG   Attr       LSize  Pool Origin  Data%
#   snap_data vg0  swi-I-s---  2.00g      lv_data 100.00
#                      ^
#                      I = Invalid (snapshot lleno)

# Un snapshot con I está corrupto — ya no es usable
# Debe eliminarse:
lvremove -f /dev/vg0/snap_data
```

### Extender un snapshot antes de que se llene

```bash
# Ver uso actual
lvs -o lv_name,origin,snap_percent /dev/vg0/snap_data
#   LV        Origin  Snap%
#   snap_data lv_data 78.55    ← ¡creciendo!

# Extender el snapshot
lvextend -L +2G /dev/vg0/snap_data
#   Size of logical volume vg0/snap_data changed from 2.00 GiB to 4.00 GiB.

# Verificar
lvs /dev/vg0/snap_data
#   snap_data  Snap%  39.28    ← el porcentaje bajó (más espacio)
```

### Autoextensión (opcional)

LVM puede extender snapshots automáticamente. Se configura en `/etc/lvm/lvm.conf`:

```bash
# En /etc/lvm/lvm.conf:
snapshot_autoextend_threshold = 80   # extender cuando llegue a 80%
snapshot_autoextend_percent = 20     # extender un 20% cada vez

# Activar el servicio de monitorización
systemctl enable --now lvm2-monitor.service
```

---

## Thin snapshots

Los snapshots tradicionales (thick) tienen el problema de que necesitas preasignar espacio. Los **thin snapshots** usan thin provisioning para evitar esto.

### Diferencia conceptual

```
Thick snapshot:
  Snapshot: [2 GiB reservados] → se llena si hay >2 GiB de cambios

Thin snapshot:
  Thin pool: [50 GiB compartidos]
  Snap 1: usa lo que necesita del pool
  Snap 2: usa lo que necesita del pool
  → múltiples snapshots, espacio compartido, sin límite individual
```

### Crear thin pool y thin LV

```bash
# Crear thin pool
lvcreate -L 20G --thinpool tp0 vg0

# Crear thin LV dentro del pool
lvcreate -V 10G --thin -n lv_data vg0/tp0

# Formatear y montar
mkfs.ext4 /dev/vg0/lv_data
mount /dev/vg0/lv_data /mnt/data
```

### Crear thin snapshot

```bash
# Snapshot de un thin LV — instantáneo, sin tamaño preasignado
lvcreate --snapshot -n snap_data /dev/vg0/lv_data
# Nota: sin -L — el espacio viene del thin pool

# Verificar
lvs
#   LV        VG   Attr       LSize  Pool Origin  Data%
#   lv_data   vg0  Vwi-a-t--- 10.00g tp0          25.00
#   snap_data vg0  Vwi---t-s- 10.00g tp0  lv_data
```

### Ventajas de thin snapshots

| Aspecto | Thick snapshot | Thin snapshot |
|---------|---------------|---------------|
| Espacio preasignado | Sí (obligatorio) | No |
| Múltiples snapshots | Costoso en espacio | Eficiente |
| Snapshot de snapshot | No | Sí |
| Rendimiento CoW | Overhead moderado | Overhead menor |
| Complejidad | Simple | Requiere thin pool |
| Merge (revertir) | Sí | Sí |

### Cuándo usar cada tipo

- **Thick**: simple, un snapshot temporal para backup o prueba rápida
- **Thin**: cuando necesitas múltiples snapshots, entornos de prueba, o snapshots frecuentes

---

## Limitaciones de los snapshots LVM

### 1. Impacto en rendimiento

```
Escritura normal (sin snapshot):
  App → Escribir bloque → disco
  1 operación de I/O

Escritura con snapshot activo (CoW):
  App → Leer bloque original → Copiar a snapshot → Escribir bloque nuevo → disco
  3 operaciones de I/O

Degradación típica: 10-30% en escrituras pesadas
```

El overhead desaparece al eliminar el snapshot.

### 2. El snapshot puede llenarse

Un thick snapshot tiene tamaño fijo. Si las escrituras en el origen exceden ese espacio, el snapshot se invalida. No hay recuperación — solo eliminarlo.

### 3. No reemplazan backups reales

```
┌───────────────────────────────────────────────────────┐
│  Snapshot ≠ Backup                                     │
│                                                       │
│  ● El snapshot reside en el MISMO VG que el origen    │
│  ● Si el disco falla, pierdes origen Y snapshot       │
│  ● Un snapshot es temporal — para operaciones cortas  │
│                                                       │
│  Backup real:                                          │
│  ● Datos en medio DIFERENTE (otro disco, remoto, nube)│
│  ● Snapshot como herramienta para obtener estado       │
│    consistente DURANTE el backup                      │
└───────────────────────────────────────────────────────┘
```

### 4. No son incrementales

Cada snapshot es independiente. No hay relación entre snapshots como en Btrfs (donde los snapshots comparten bloques nativamente). Crear 5 thick snapshots de 2 GiB consume 10 GiB del VG.

### 5. Limitación de número

Con thick snapshots, cada snapshot activo añade overhead. Tener más de 3-5 thick snapshots activos simultáneamente puede degradar significativamente el rendimiento. Los thin snapshots son mucho mejores en este aspecto.

### Comparación con otros mecanismos

```
┌────────────────┬──────────────┬──────────────┬──────────────┐
│                │ LVM snapshot │ Btrfs snap   │ QEMU snap    │
├────────────────┼──────────────┼──────────────┼──────────────┤
│ Granularidad   │ LV completo  │ Subvolumen   │ Disco VM     │
│ Rendimiento    │ CoW overhead │ Nativo CoW   │ Depende      │
│ Múltiples      │ Costoso      │ Eficiente    │ Cadena       │
│ Revertir       │ merge        │ mv/btrfs sub │ snapshot-rev │
│ Independiente  │ No           │ Sí           │ No           │
│ Incremental    │ No           │ Sí (send)    │ No           │
└────────────────┴──────────────┴──────────────┴──────────────┘
```

---

## Errores comunes

### 1. Snapshot demasiado pequeño

```bash
# ✗ Snapshot de 100 MiB para un LV con escrituras intensivas
lvcreate --snapshot -L 100M -n snap /dev/vg0/lv_data
# Se llena en minutos → snapshot invalidado

# ✓ Asignar suficiente espacio según el uso esperado
# Para un backup que tarda 30 min, con 50 MB/min de escrituras:
# 30 × 50 = 1500 MB mínimo
lvcreate --snapshot -L 2G -n snap /dev/vg0/lv_data
```

### 2. Olvidar eliminar snapshots temporales

```bash
# ✗ Crear snapshot para backup y olvidar eliminarlo
# Días después: snapshot al 100%, rendimiento degradado

# ✓ Siempre eliminar después de usar
umount /mnt/snap
lvremove -f /dev/vg0/snap

# ✓✓ Automatizar en un script de backup
lvcreate --snapshot -L 2G -n snap /dev/vg0/lv_data
mount -o ro /dev/vg0/snap /mnt/snap
tar czf /backup/data.tar.gz -C /mnt/snap .
umount /mnt/snap
lvremove -f /dev/vg0/snap    # limpieza automática
```

### 3. Montar snapshot XFS sin nouuid

```bash
# ✗ UUID duplicado
mount /dev/vg0/snap_data /mnt/snap
# mount: wrong fs type... (XFS rechaza UUID duplicado)

# ✓ Usar nouuid
mount -o ro,nouuid /dev/vg0/snap_data /mnt/snap
```

### 4. Confundir snapshot con backup

```bash
# ✗ "Tengo un snapshot, estoy protegido"
# Si el disco falla, snapshot y origen se pierden juntos

# ✓ Usar snapshot como paso DENTRO de un proceso de backup
# snapshot → montar → copiar a OTRO medio → eliminar snapshot
```

### 5. Hacer merge sin desmontar

```bash
# ✗ Merge con el origen montado (en LV no-root)
lvconvert --merge /dev/vg0/snap_data
# Can't merge until origin volume is closed

# ✓ Desmontar primero
umount /mnt/data
umount /mnt/snap  # si estaba montado
lvconvert --merge /dev/vg0/snap_data
mount /dev/vg0/lv_data /mnt/data
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│                Snapshots LVM — Referencia rápida                 │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  CREAR:                                                          │
│    lvcreate --snapshot -L 2G -n snap /dev/vg0/lv_data            │
│    (thick — tamaño preasignado)                                  │
│                                                                  │
│    lvcreate --snapshot -n snap /dev/vg0/lv_data                  │
│    (thin — si lv_data es un thin LV)                             │
│                                                                  │
│  MONTAR:                                                         │
│    mount -o ro /dev/vg0/snap /mnt/snap           ext4            │
│    mount -o ro,nouuid /dev/vg0/snap /mnt/snap    XFS             │
│                                                                  │
│  INSPECCIONAR:                                                   │
│    lvs                     ver Data% (uso del snapshot)          │
│    lvs -o +snap_percent    columna explícita de porcentaje       │
│                                                                  │
│  EXTENDER (si se está llenando):                                 │
│    lvextend -L +2G /dev/vg0/snap                                 │
│                                                                  │
│  REVERTIR (merge):                                               │
│    umount /mnt/data                                              │
│    umount /mnt/snap                                              │
│    lvconvert --merge /dev/vg0/snap                               │
│    mount /dev/vg0/lv_data /mnt/data                              │
│    (rootfs: merge ocurre en el próximo reboot)                   │
│                                                                  │
│  ELIMINAR:                                                       │
│    umount /mnt/snap                                              │
│    lvremove -f /dev/vg0/snap                                     │
│                                                                  │
│  FLUJO BACKUP:                                                   │
│    lvcreate --snapshot → mount -o ro → tar/rsync → umount →     │
│    lvremove                                                      │
│                                                                  │
│  FLUJO PRUEBA:                                                   │
│    lvcreate --snapshot → hacer cambios → ¿OK? → lvremove         │
│                                          ¿NO? → merge (revertir)│
│                                                                  │
│  TAMAÑO RECOMENDADO:                                             │
│    Backup (minutos): 5-10% del origen                            │
│    Prueba (horas):   20-30%                                      │
│    Largo plazo:      50-100%                                     │
│                                                                  │
│  ATRIBUTOS lvs:                                                  │
│    o = origin    s = snapshot    I = Invalid (lleno)              │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Snapshot para backup

En tu VM de lab:

1. Crea el stack LVM:
   ```bash
   pvcreate /dev/vdb /dev/vdc
   vgcreate vg_lab /dev/vdb /dev/vdc
   lvcreate -L 500M -n lv_data vg_lab
   mkfs.ext4 /dev/vg_lab/lv_data
   mkdir -p /mnt/data
   mount /dev/vg_lab/lv_data /mnt/data
   ```
2. Crea datos de prueba:
   ```bash
   echo "important data" > /mnt/data/config.txt
   dd if=/dev/urandom of=/mnt/data/bigfile bs=1M count=50
   md5sum /mnt/data/bigfile > /tmp/checksum.txt
   ```
3. Crea un snapshot de 200 MiB: `lvcreate --snapshot -L 200M -n snap_data /dev/vg_lab/lv_data`
4. Verifica con `lvs` — ¿qué porcentaje muestra Data%?
5. Monta el snapshot: `mount -o ro /dev/vg_lab/snap_data /mnt/snap`
6. Modifica datos en el **origen**: `echo "modified" > /mnt/data/config.txt`
7. Compara: `cat /mnt/data/config.txt` vs `cat /mnt/snap/config.txt` — ¿son distintos?
8. Verifica `lvs` de nuevo — ¿cambió Data%?
9. Haz un backup: `tar czf /tmp/backup.tar.gz -C /mnt/snap .`
10. Limpia: `umount /mnt/snap && lvremove -f /dev/vg_lab/snap_data`

> **Pregunta de reflexión**: si el backup tarda 10 minutos y la aplicación escribe 5 MiB/min al filesystem, ¿son suficientes 200 MiB de snapshot? ¿Qué pasa si calculas mal y el snapshot se llena durante el backup?

### Ejercicio 2: Revertir con merge

Continuando del ejercicio anterior:

1. Verifica el estado actual: `cat /mnt/data/config.txt` (debería decir "modified")
2. Crea un nuevo snapshot: `lvcreate --snapshot -L 200M -n snap_data /dev/vg_lab/lv_data`
3. Haz cambios destructivos en el origen:
   ```bash
   rm /mnt/data/bigfile
   echo "broken" > /mnt/data/config.txt
   ```
4. Verifica: `ls /mnt/data/` — bigfile ya no está
5. Ahora revierte:
   ```bash
   umount /mnt/data
   lvconvert --merge /dev/vg_lab/snap_data
   mount /dev/vg_lab/lv_data /mnt/data
   ```
6. Verifica: `cat /mnt/data/config.txt` — ¿volvió al estado anterior?
7. Verifica: `ls /mnt/data/bigfile` — ¿reapareció?
8. Verifica `lvs` — ¿el snapshot desapareció?

> **Pregunta de reflexión**: ¿se puede hacer merge de un snapshot que a su vez tiene un snapshot? ¿Qué limitaciones impone LVM en la cadena de snapshots?

### Ejercicio 3: Llenar y extender un snapshot

1. Crea un snapshot pequeño: `lvcreate --snapshot -L 50M -n snap_small /dev/vg_lab/lv_data`
2. Verifica Data%: `lvs /dev/vg_lab/snap_small`
3. Escribe datos en el **origen** para llenar el snapshot:
   ```bash
   dd if=/dev/urandom of=/mnt/data/fill bs=1M count=30
   ```
4. Monitoriza Data% mientras escribes: `watch lvs /dev/vg_lab/snap_small`
5. Si Data% supera 70%, extiende: `lvextend -L +100M /dev/vg_lab/snap_small`
6. Sigue escribiendo hasta que Data% se acerque a 100% sin extender — ¿qué pasa cuando llega a 100%?
7. Verifica el atributo con `lvs` — ¿ves la `I` de Invalid?
8. Elimina el snapshot invalidado: `lvremove -f /dev/vg_lab/snap_small`
9. Limpieza total: `umount /mnt/data && vgremove -f vg_lab && pvremove /dev/vdb /dev/vdc`

> **Pregunta de reflexión**: ¿por qué LVM invalida el snapshot en lugar de pausar las escrituras en el LV original? ¿Qué impacto tendría en producción si LVM pausara las escrituras?
