# ext4

## Índice

1. [Qué es ext4](#1-qué-es-ext4)
2. [Crear un filesystem ext4](#2-crear-un-filesystem-ext4)
3. [Estructura interna](#3-estructura-interna)
4. [tune2fs: ajustar parámetros](#4-tune2fs-ajustar-parámetros)
5. [El journal](#5-el-journal)
6. [Features de ext4](#6-features-de-ext4)
7. [dumpe2fs: inspeccionar el filesystem](#7-dumpe2fs-inspeccionar-el-filesystem)
8. [Redimensionar ext4](#8-redimensionar-ext4)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Qué es ext4

ext4 (fourth extended filesystem) es el filesystem por defecto en Debian, Ubuntu
y la mayoría de distribuciones Linux. Es la evolución de ext2 → ext3 → ext4:

```
┌────────────────────────────────────────────────────────────┐
│  Evolución:                                                │
│                                                            │
│  ext2 (1993)  →  ext3 (2001)  →  ext4 (2008)              │
│  sin journal     + journal       + extents                 │
│                                  + tamaño mayor            │
│                                  + timestamps nanoseg.     │
│                                  + preallocación           │
│                                  + checksums en journal    │
└────────────────────────────────────────────────────────────┘
```

### Características principales

| Característica | Valor |
|----------------|-------|
| Tamaño máximo de archivo | 16 TiB |
| Tamaño máximo de filesystem | 1 EiB |
| Número máximo de archivos | ~4 mil millones |
| Longitud máxima de nombre | 255 bytes |
| Journaling | Sí (ordered por defecto) |
| Extents | Sí (reemplazan block mapping) |
| Online resize | Sí (crecer, no encoger) |
| Soporte en kernel | Nativo, muy maduro |

### Cuándo usar ext4

```
✓ Filesystem por defecto — si no tienes razón para elegir otro
✓ Partición root (/)
✓ Particiones de datos generales
✓ Cuando necesitas máxima compatibilidad y estabilidad
✓ Debian y derivados (default del instalador)

✗ Cuando necesitas snapshots integrados → Btrfs
✗ Cuando necesitas máximo rendimiento en archivos enormes → XFS
✗ Almacenamiento empresarial RHEL → XFS (default de RHEL)
```

---

## 2. Crear un filesystem ext4

### 2.1. mkfs.ext4 básico

```bash
sudo mkfs.ext4 /dev/vdb1
```

```
mke2fs 1.47.0 (5-Feb-2023)
Creating filesystem with 262144 4k blocks and 65536 inodes
Filesystem UUID: a1b2c3d4-e5f6-7890-abcd-ef1234567890
Superblock backups stored on blocks:
        32768, 98304, 163840, 229376

Allocating group descriptors: done
Writing inode tables: done
Creating journal (8192 blocks): done
Writing superblocks and filesystem accounting information: done
```

**Predicción**: el comando tarda menos de un segundo en un disco de 2G. Crea
las estructuras internas (superbloque, tablas de inodos, journal) y genera un
UUID único.

### 2.2. Opciones de mkfs.ext4

```bash
# Con label
sudo mkfs.ext4 -L "datos" /dev/vdb1

# Con porcentaje de bloques reservados (default: 5%)
sudo mkfs.ext4 -m 1 /dev/vdb1        # solo 1% reservado

# Con tamaño de bloque específico
sudo mkfs.ext4 -b 4096 /dev/vdb1     # 4K (default en la mayoría de casos)

# Con número de inodos personalizado
sudo mkfs.ext4 -N 1000000 /dev/vdb1  # más inodos para muchos archivos pequeños

# Forzar creación (sin confirmación)
sudo mkfs.ext4 -F /dev/vdb           # en disco completo (sin partición)
```

### 2.3. Opciones más comunes

| Opción | Significado | Default | Cuándo cambiar |
|--------|-------------|---------|----------------|
| `-L label` | Etiqueta del filesystem | (ninguna) | Siempre — útil para identificar |
| `-m N` | % bloques reservados para root | 5% | Discos grandes o de datos: reducir a 1% |
| `-b SIZE` | Tamaño de bloque (bytes) | 4096 | Rara vez — 4K es óptimo |
| `-N count` | Número de inodos | Automático | Muchos archivos pequeños: aumentar |
| `-O features` | Features a habilitar | Default set | Avanzado |
| `-F` | Forzar (sin confirmación) | No | Scripts, disco sin partición |

### 2.4. Bloques reservados: el 5% para root

Por defecto, ext4 reserva 5% del espacio para el usuario root. Esto previene
que el filesystem se llene al 100%, lo cual puede causar problemas:

```
Disco de 100G:
  5% reservado = 5G que solo root puede usar
  Para usuario normal: "disco lleno" al 95%

  ¿Por qué?
  - Si / se llena al 100%, el sistema puede no arrancar
  - root necesita poder escribir logs y limpiar
  - Fragmentación aumenta drásticamente al 100%
```

Para discos de datos (no `/`), reducir a 1% o 0%:

```bash
# Al crear
sudo mkfs.ext4 -m 1 /dev/vdb1

# Después de crear
sudo tune2fs -m 0 /dev/vdb1
```

---

## 3. Estructura interna

### 3.1. Componentes de ext4

```
┌──────────────────────────────────────────────────────────────┐
│                   FILESYSTEM EXT4                            │
│                                                              │
│  ┌───────────┐  ┌─────────────────────────────────────────┐  │
│  │Superblock │  │           Block Groups                  │  │
│  │           │  │  ┌────────┬────────┬────────┬────────┐  │  │
│  │ - UUID    │  │  │Group 0 │Group 1 │Group 2 │  ...   │  │  │
│  │ - tamaño  │  │  │        │        │        │        │  │  │
│  │ - block sz│  │  │┌──────┐│┌──────┐│┌──────┐│        │  │  │
│  │ - features│  │  ││Inode ││Inode ││Inode ││        │  │  │
│  │ - estado  │  │  ││Table ││Table ││Table ││        │  │  │
│  │ - mount # │  │  │├──────┤│├──────┤│├──────┤│        │  │  │
│  │           │  │  ││Data  ││Data  ││Data  ││        │  │  │
│  │ Copias en │  │  ││Blocks││Blocks││Blocks││        │  │  │
│  │ cada group│  │  │└──────┘│└──────┘│└──────┘│        │  │  │
│  └───────────┘  │  └────────┴────────┴────────┴────────┘  │  │
│                 └─────────────────────────────────────────┘  │
│                                                              │
│  ┌───────────┐                                               │
│  │  Journal  │  Registro de transacciones pendientes         │
│  └───────────┘                                               │
└──────────────────────────────────────────────────────────────┘
```

### 3.2. Superblock

El superblock contiene los metadatos del filesystem:

- UUID, label, tamaño
- Tamaño de bloque (4096 bytes típicamente)
- Conteo de inodos y bloques (totales, libres, reservados)
- Estado del filesystem (clean, errors)
- Conteo de montajes y fecha de último check
- Features habilitadas

Hay copias de respaldo del superblock en varios block groups. Si el primario
se corrompe, se puede recuperar desde una copia.

### 3.3. Inodos

Cada archivo o directorio tiene un **inodo** que contiene sus metadatos:

| Campo del inodo | Contenido |
|-----------------|-----------|
| Permisos | rwxr-xr-x |
| Propietario | UID, GID |
| Timestamps | atime, mtime, ctime, crtime |
| Tamaño | En bytes |
| Conteo de enlaces | Número de hard links |
| Extents/bloques | Dónde están los datos |
| Tipo | Archivo regular, directorio, symlink... |

**No contiene**: el nombre del archivo. Los nombres están en las entradas de
directorio, que apuntan al inodo. Por eso un archivo puede tener varios nombres
(hard links).

```bash
# Ver el inodo de un archivo
stat /etc/hostname
# o
ls -i /etc/hostname
# 12345 /etc/hostname    ← 12345 es el número de inodo
```

### 3.4. Extents vs block mapping

ext4 usa **extents** para mapear datos de archivos. Un extent describe un rango
continuo de bloques:

```
ext2/ext3 (block mapping):        ext4 (extents):
  bloque 100                        extent: inicio=100, longitud=5
  bloque 101                        (describe los 5 bloques de una vez)
  bloque 102
  bloque 103
  bloque 104

  5 entradas para 5 bloques         1 entrada para 5 bloques
```

Los extents son más eficientes para archivos grandes y reducen la fragmentación.

---

## 4. tune2fs: ajustar parámetros

`tune2fs` modifica parámetros de un filesystem ext2/ext3/ext4 **sin formatearlo**.

### 4.1. Ver parámetros actuales

```bash
sudo tune2fs -l /dev/vdb1
```

```
tune2fs 1.47.0 (5-Feb-2023)
Filesystem volume name:   datos
Last mounted on:          <not available>
Filesystem UUID:          a1b2c3d4-e5f6-7890-abcd-ef1234567890
Filesystem magic number:  0xEF53
Filesystem revision #:    1 (dynamic)
Filesystem features:      has_journal ext_attr resize_inode dir_index
                          filetype needs_recovery extent 64bit flex_bg
                          sparse_super large_file huge_file dir_nlink
                          extra_isize metadata_csum
Filesystem flags:         signed_directory_hash
Default mount options:    user_xattr acl
Filesystem state:         clean
Errors behavior:          Continue
...
Inode count:              65536
Block count:              262144
Reserved block count:     13107
Free blocks:              245605
Free inodes:              65525
Block size:               4096
...
```

### 4.2. Modificar parámetros

```bash
# Cambiar label
sudo tune2fs -L "new-label" /dev/vdb1

# Cambiar bloques reservados (porcentaje)
sudo tune2fs -m 1 /dev/vdb1

# Cambiar bloques reservados (número absoluto)
sudo tune2fs -r 1000 /dev/vdb1

# Cambiar UUID (generar nuevo)
sudo tune2fs -U random /dev/vdb1

# Cambiar UUID (específico)
sudo tune2fs -U "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" /dev/vdb1

# Cambiar conteo de montajes para check (0 = deshabilitado)
sudo tune2fs -c 0 /dev/vdb1

# Cambiar intervalo de check (0 = deshabilitado)
sudo tune2fs -i 0 /dev/vdb1

# Cambiar comportamiento ante errores
sudo tune2fs -e remount-ro /dev/vdb1    # remount read-only
sudo tune2fs -e continue /dev/vdb1      # continuar
sudo tune2fs -e panic /dev/vdb1         # kernel panic
```

### 4.3. Cambiar label: tune2fs vs e2label

```bash
# Son equivalentes:
sudo tune2fs -L "datos" /dev/vdb1
sudo e2label /dev/vdb1 "datos"

# Ver label actual:
sudo e2label /dev/vdb1
# datos
```

### 4.4. Features: habilitar/deshabilitar

```bash
# Habilitar una feature
sudo tune2fs -O has_journal /dev/vdb1

# Deshabilitar una feature
sudo tune2fs -O ^has_journal /dev/vdb1    # ← convierte ext4 en ext2

# Ver features activas
sudo tune2fs -l /dev/vdb1 | grep features
```

---

## 5. El journal

### 5.1. Qué es el journal

El journal es un registro de transacciones que protege la integridad del
filesystem ante apagones o crashes:

```
Sin journal (ext2):
┌────────────────────────────────────────────────┐
│  1. Escribir datos a disco                     │
│  2. Actualizar metadatos (inodo, directorio)   │
│                                                │
│  Si el sistema cae entre 1 y 2:                │
│    → Datos escritos pero metadatos inconsistentes│
│    → fsck necesita revisar TODO el disco         │
│    → Puede tardar horas en discos grandes        │
└────────────────────────────────────────────────┘

Con journal (ext3/ext4):
┌────────────────────────────────────────────────┐
│  1. Escribir intención en el journal           │
│  2. Escribir datos a disco                     │
│  3. Actualizar metadatos                       │
│  4. Marcar transacción como completada         │
│                                                │
│  Si el sistema cae en cualquier punto:          │
│    → Al arrancar, solo se revisa el journal     │
│    → Transacciones incompletas se deshacen       │
│    → Recovery en segundos, no horas             │
└────────────────────────────────────────────────┘
```

### 5.2. Modos de journaling

ext4 soporta 3 modos de journal, configurables al montar:

| Modo | Qué se escribe al journal | Rendimiento | Seguridad |
|------|---------------------------|-------------|-----------|
| `journal` | Metadatos + datos | Más lento | Máxima |
| `ordered` | Solo metadatos (datos se escriben primero) | **Default** | Alta |
| `writeback` | Solo metadatos (datos en cualquier orden) | Más rápido | Menor |

```bash
# Montar con modo journal específico
sudo mount -o data=journal /dev/vdb1 /mnt      # máxima seguridad
sudo mount -o data=ordered /dev/vdb1 /mnt      # default
sudo mount -o data=writeback /dev/vdb1 /mnt    # máximo rendimiento
```

### 5.3. Tamaño del journal

```bash
# Ver tamaño del journal
sudo dumpe2fs /dev/vdb1 2>/dev/null | grep "Journal size"
# Journal size:             32M

# El journal típicamente ocupa 32-128 MiB
# Se crea automáticamente con mkfs.ext4
```

### 5.4. Deshabilitar el journal

```bash
# Desmonta primero
sudo umount /dev/vdb1

# Deshabilitar (convierte efectivamente a ext2)
sudo tune2fs -O ^has_journal /dev/vdb1

# Habilitar de nuevo
sudo tune2fs -O has_journal /dev/vdb1
```

Solo se justifica deshabilitar el journal en SSDs de lab para reducir escrituras,
o en situaciones muy específicas. En producción, **siempre tener journal activo**.

---

## 6. Features de ext4

### 6.1. Features principales

```bash
sudo tune2fs -l /dev/vdb1 | grep "Filesystem features"
```

| Feature | Significado |
|---------|-------------|
| `has_journal` | Journaling activo |
| `extent` | Usa extents (no block mapping) |
| `64bit` | Soporta filesystems > 16 TiB |
| `flex_bg` | Block groups flexibles (mejor rendimiento) |
| `metadata_csum` | Checksums en metadatos (integridad) |
| `dir_index` | Índice hash en directorios (búsqueda rápida) |
| `huge_file` | Soporta archivos > 2 TiB |
| `sparse_super` | Superbloques de respaldo solo en algunos groups |
| `resize_inode` | Permite crecimiento online |
| `extra_isize` | Inodos extendidos (timestamps nanosegundo) |

### 6.2. Verificar una feature específica

```bash
# ¿Tiene journal?
sudo tune2fs -l /dev/vdb1 | grep has_journal

# ¿Usa extents?
sudo tune2fs -l /dev/vdb1 | grep extent

# ¿Tiene checksums de metadatos?
sudo tune2fs -l /dev/vdb1 | grep metadata_csum
```

---

## 7. dumpe2fs: inspeccionar el filesystem

`dumpe2fs` muestra información detallada del filesystem:

### 7.1. Información del superblock

```bash
sudo dumpe2fs /dev/vdb1 2>/dev/null | head -50
```

```
Filesystem volume name:   datos
Filesystem UUID:          a1b2c3d4-...
Filesystem state:         clean
Inode count:              65536
Block count:              262144
Reserved block count:     13107
Free blocks:              245605
Free inodes:              65525
First block:              0
Block size:               4096
Fragment size:            4096
Journal UUID:             <none>
Journal inode:            8
Journal size:             32M
```

### 7.2. Información útil que extraer

```bash
# Espacio libre
sudo dumpe2fs /dev/vdb1 2>/dev/null | grep "Free blocks"
# Free blocks:              245605

# Inodos libres
sudo dumpe2fs /dev/vdb1 2>/dev/null | grep "Free inodes"
# Free inodes:              65525

# Estado del filesystem
sudo dumpe2fs /dev/vdb1 2>/dev/null | grep "Filesystem state"
# Filesystem state:         clean

# Último montaje
sudo dumpe2fs /dev/vdb1 2>/dev/null | grep "Last mount"

# Conteo de montajes
sudo dumpe2fs /dev/vdb1 2>/dev/null | grep "Mount count"
```

### 7.3. Ubicación de superbloques de respaldo

```bash
sudo dumpe2fs /dev/vdb1 2>/dev/null | grep -i superblock
# Primary superblock at 0, ...
# Backup superblock at 32768, ...
# Backup superblock at 98304, ...
```

Esto es crítico si necesitas reparar un filesystem con el superbloque corrupto:

```bash
# Reparar usando un superbloque de respaldo
sudo e2fsck -b 32768 /dev/vdb1
```

---

## 8. Redimensionar ext4

### 8.1. Crecer (online)

ext4 puede crecer **mientras está montado**:

```bash
# 1. Ampliar la partición (con parted o fdisk)
sudo parted -s /dev/vdb resizepart 1 100%

# 2. Ampliar el filesystem (puede estar montado)
sudo resize2fs /dev/vdb1
# resize2fs detecta automáticamente el nuevo tamaño de la partición
```

```bash
# O especificar un tamaño
sudo resize2fs /dev/vdb1 1500M     # crecer a 1500 MiB
```

### 8.2. Encoger (offline)

ext4 puede encogerse, pero **debe estar desmontado**:

```bash
# 1. Desmontar
sudo umount /mnt

# 2. Verificar el filesystem
sudo e2fsck -f /dev/vdb1

# 3. Encoger el filesystem
sudo resize2fs /dev/vdb1 500M

# 4. Encoger la partición (con parted o fdisk)
# CUIDADO: el nuevo tamaño debe ser >= al filesystem
sudo parted /dev/vdb resizepart 1 500MiB
```

```
⚠ ORDEN DE OPERACIONES

  CRECER:
    1. Ampliar la partición       ← primero el contenedor
    2. resize2fs                  ← luego el contenido

  ENCOGER:
    1. resize2fs                  ← primero el contenido
    2. Encoger la partición       ← luego el contenedor

  Si encoges la partición antes del filesystem → PÉRDIDA DE DATOS
```

---

## 9. Errores comunes

### Error 1: formatear la partición equivocada

```bash
# ✗ ¡Verificar SIEMPRE antes de mkfs!
sudo mkfs.ext4 /dev/vda1             # ← disco del SO, destruye todo

# ✓ Verificar qué es cada dispositivo
lsblk
sudo mkfs.ext4 /dev/vdb1             # ← disco extra, correcto
```

`mkfs` **destruye todos los datos** del dispositivo. No hay confirmación por
defecto (se ejecuta inmediatamente).

### Error 2: no reducir el filesystem antes de reducir la partición

```bash
# ✗ Encoger partición sin encoger filesystem
sudo parted -s /dev/vdb resizepart 1 500MiB
# Los datos al final del filesystem quedan fuera de la partición
# → corrupción

# ✓ Encoger filesystem PRIMERO
sudo umount /mnt
sudo e2fsck -f /dev/vdb1
sudo resize2fs /dev/vdb1 400M        # un poco menos que la partición
sudo parted -s /dev/vdb resizepart 1 500MiB
```

### Error 3: no saber cuánto espacio pierde el 5% reservado

```bash
# Disco de 2TB con 5% reservado:
# 2TB × 5% = 100GB reservados para root ← ¡mucho!

# ✓ Para discos de datos, reducir:
sudo mkfs.ext4 -m 0 /dev/vdb1        # 0% reservado
# o después:
sudo tune2fs -m 1 /dev/vdb1          # 1% reservado
```

### Error 4: quedarse sin inodos

```bash
# Si creas millones de archivos pequeños:
df -i /mnt
# Filesystem     Inodes  IUsed   IFree IUse% Mounted on
# /dev/vdb1       65536  65536       0  100% /mnt
# ¡Sin inodos libres! No puedes crear más archivos aunque haya espacio

# ✓ Para muchos archivos pequeños, crear con más inodos:
sudo mkfs.ext4 -N 500000 /dev/vdb1
# o con ratio bytes/inodo menor:
sudo mkfs.ext4 -i 1024 /dev/vdb1     # 1 inodo cada 1KB
```

### Error 5: confundir tune2fs con mkfs

```bash
# mkfs.ext4:  CREA un filesystem nuevo (destruye datos)
# tune2fs:    MODIFICA parámetros de un filesystem existente (no destruye)

# ✗ "Quiero cambiar el label" — NO usar mkfs
sudo mkfs.ext4 -L "nuevo" /dev/vdb1   # ← destruye TODO

# ✓ Usar tune2fs
sudo tune2fs -L "nuevo" /dev/vdb1     # ← solo cambia el label
```

---

## 10. Cheatsheet

```bash
# ── Crear filesystem ────────────────────────────────────────────
sudo mkfs.ext4 /dev/vdb1                    # básico
sudo mkfs.ext4 -L "label" /dev/vdb1         # con label
sudo mkfs.ext4 -m 1 /dev/vdb1              # 1% reservado
sudo mkfs.ext4 -m 1 -L "datos" /dev/vdb1   # combinado

# ── Inspeccionar ────────────────────────────────────────────────
sudo tune2fs -l /dev/vdb1                   # parámetros
sudo dumpe2fs /dev/vdb1 2>/dev/null | head  # detalle completo
sudo e2label /dev/vdb1                      # ver label
sudo blkid /dev/vdb1                        # UUID y tipo

# ── Modificar parámetros ────────────────────────────────────────
sudo tune2fs -L "nuevo-label" /dev/vdb1     # cambiar label
sudo tune2fs -m 1 /dev/vdb1                # cambiar % reservado
sudo tune2fs -U random /dev/vdb1           # generar nuevo UUID
sudo tune2fs -c 0 -i 0 /dev/vdb1           # deshabili. check periódico
sudo tune2fs -e remount-ro /dev/vdb1       # remount-ro ante errores

# ── Redimensionar ───────────────────────────────────────────────
# Crecer (online):
sudo resize2fs /dev/vdb1                    # al tamaño de la partición
sudo resize2fs /dev/vdb1 1500M              # a tamaño específico

# Encoger (offline):
sudo umount /mnt
sudo e2fsck -f /dev/vdb1                    # obligatorio antes de encoger
sudo resize2fs /dev/vdb1 500M               # encoger filesystem
# Luego encoger la partición con parted

# ── Journal ─────────────────────────────────────────────────────
# Modos de montaje:
sudo mount -o data=ordered /dev/vdb1 /mnt   # default
sudo mount -o data=journal /dev/vdb1 /mnt   # más seguro
sudo mount -o data=writeback /dev/vdb1 /mnt # más rápido

# Deshabilitar/habilitar:
sudo tune2fs -O ^has_journal /dev/vdb1      # deshabilitar
sudo tune2fs -O has_journal /dev/vdb1       # habilitar

# ── Reparación (ver T03 de S02) ─────────────────────────────────
sudo e2fsck -f /dev/vdb1                    # check forzado
sudo e2fsck -b 32768 /dev/vdb1             # con superbloque backup
```

---

## 11. Ejercicios

### Ejercicio 1: crear y explorar un filesystem ext4

1. Crea una partición de 1G en `/dev/vdb` (con fdisk o parted)
2. Crea un filesystem ext4 con label "lab-ext4": `sudo mkfs.ext4 -L "lab-ext4" /dev/vdb1`
3. Examina los parámetros con `sudo tune2fs -l /dev/vdb1`:
   - ¿Cuántos inodos tiene? ¿Y bloques?
   - ¿Cuántos bloques están reservados? ¿Qué porcentaje es?
   - ¿Qué features están habilitadas?
4. Verifica el UUID con `sudo blkid /dev/vdb1`
5. Monta en `/mnt`, crea algunos archivos, verifica con `df -h /mnt`
6. ¿Cuánto espacio "usable" tienes de los 1G? ¿Adónde fue el resto?

**Pregunta de reflexión**: un filesystem ext4 de 1G recién creado ya muestra
~24MB usados (antes de guardar cualquier archivo). ¿Qué ocupan esos 24MB?
Pista: tablas de inodos, journal, superbloque y sus copias.

### Ejercicio 2: tune2fs

1. Con el filesystem del ejercicio 1 desmontado:
   - Cambia el label a "modified" con `tune2fs -L`
   - Reduce los bloques reservados a 1% con `tune2fs -m 1`
   - Genera un nuevo UUID con `tune2fs -U random`
2. Verifica cada cambio con `tune2fs -l`
3. Monta de nuevo y compara `df -h` — ¿cambió el espacio disponible?

**Pregunta de reflexión**: cambiaste el UUID del filesystem. Si este filesystem
estuviera referenciado en `/etc/fstab` por UUID, ¿qué pasaría en el siguiente
reboot?

### Ejercicio 3: redimensionar ext4

1. Crea una partición de 500M en `/dev/vdc` y formatea con ext4
2. Monta, crea un archivo de prueba, verifica con `df -h`
3. Desmonta
4. Amplía la partición a todo el disco (2G) con parted
5. Monta de nuevo — ¿el tamaño cambió en `df -h`?
6. Ejecuta `sudo resize2fs /dev/vdc1` (con el fs montado)
7. Verifica con `df -h` — ahora debería mostrar ~2G
8. Limpia: desmonta, borra particiones

**Pregunta de reflexión**: pudiste ejecutar `resize2fs` con el filesystem
montado. ¿Por qué ext4 permite crecer online pero no encoger online? ¿Qué
riesgos tendría un encogimiento con archivos abiertos?
