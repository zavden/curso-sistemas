# Sistemas de Archivos

## Índice

1. [Qué es un sistema de archivos](#1-qué-es-un-sistema-de-archivos)
2. [Anatomía interna: superbloque, inodos y bloques de datos](#2-anatomía-interna-superbloque-inodos-y-bloques-de-datos)
3. [Journaling: protección contra corrupción](#3-journaling-protección-contra-corrupción)
4. [ext4: el estándar de facto](#4-ext4-el-estándar-de-facto)
5. [XFS: alto rendimiento](#5-xfs-alto-rendimiento)
6. [Btrfs: copy-on-write y snapshots nativos](#6-btrfs-copy-on-write-y-snapshots-nativos)
7. [Otros filesystems relevantes](#7-otros-filesystems-relevantes)
8. [mkfs: crear un filesystem](#8-mkfs-crear-un-filesystem)
9. [fsck: verificar y reparar](#9-fsck-verificar-y-reparar)
10. [Comparación y criterios de elección](#10-comparación-y-criterios-de-elección)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Qué es un sistema de archivos

En T02 creaste particiones — divisiones del disco. Pero una partición recién creada es una secuencia de sectores vacíos. No tiene concepto de "archivos", "directorios" ni "permisos". El **sistema de archivos** (filesystem) es la estructura de datos que organiza esos sectores en algo utilizable:

```
 Partición sin filesystem              Partición con ext4
┌─────────────────────────┐     ┌───────────────────────────────┐
│ 00000000000000000000000 │     │ Superbloque │ Tabla de inodos │
│ 00000000000000000000000 │     ├─────────────┼─────────────────┤
│ (sectores vacíos, no se │     │ Inodo 1: /                    │
│  puede montar ni usar)  │     │ Inodo 2: /etc/                │
│                         │     │ Inodo 3: /etc/hosts → blk 42  │
│                         │     │ Bloques de datos [42][43]...   │
└─────────────────────────┘     └───────────────────────────────┘
                                  ↑ Ahora se puede montar y usar
```

Un filesystem resuelve tres problemas fundamentales:

1. **¿Dónde están los datos?** — Mapea nombres de archivo a bloques físicos en el disco.
2. **¿Qué metadatos tiene cada archivo?** — Permisos, dueño, timestamps, tamaño.
3. **¿Cómo se recupera ante un fallo?** — Journaling, checksums, copy-on-write.

---

## 2. Anatomía interna: superbloque, inodos y bloques de datos

Todos los filesystems de la familia ext (ext2/3/4) y conceptualmente XFS usan la misma arquitectura fundamental con tres componentes:

### Superbloque

El superbloque es la **ficha técnica** del filesystem. Contiene metadatos globales:

```
 Superbloque (ejemplo ext4)
┌──────────────────────────────────────┐
│ Total de inodos:        1,310,720    │
│ Total de bloques:       5,242,880    │
│ Tamaño de bloque:       4096 bytes   │
│ Bloques libres:         4,891,234    │
│ Inodos libres:          1,308,200    │
│ Último montaje:         2026-03-19   │
│ Cuenta de montajes:     42           │
│ UUID: a1b2c3d4-...                   │
│ Estado: clean                        │
└──────────────────────────────────────┘
```

El superbloque se replica en varios puntos del disco (copias de seguridad). Si el superbloque principal se corrompe, `fsck` puede usar una copia para recuperar:

```bash
# Ver dónde están las copias del superbloque (ext4)
sudo dumpe2fs /dev/vdb1 | grep -i superblock
# Primary superblock at 0, ...
# Backup superblock at 32768, ...
# Backup superblock at 98304, ...
```

### Inodos (index nodes)

Cada archivo y directorio tiene un **inodo** — una estructura que almacena **todo excepto el nombre y el contenido**:

```
 Inodo #1042
┌──────────────────────────────────┐
│ Tipo:        archivo regular     │
│ Permisos:    -rw-r--r--         │
│ Owner:       1000 (zavden)       │
│ Group:       1000 (zavden)       │
│ Tamaño:      8192 bytes         │
│ Link count:  1                   │
│ atime:       2026-03-19 10:30   │
│ mtime:       2026-03-18 15:00   │
│ ctime:       2026-03-18 15:00   │
│ Bloques:     [blk 500] [blk 501]│
│              ↑ punteros a datos  │
└──────────────────────────────────┘
```

Observaciones clave:
- El **nombre del archivo NO está en el inodo**. Está en la entrada del directorio padre, que mapea nombre → número de inodo.
- Un **hard link** es otra entrada de directorio que apunta al mismo inodo (incrementa link count).
- El **número de inodos se fija al crear el filesystem**. Si se agotan, no puedes crear más archivos aunque haya espacio libre en disco.

```bash
# Ver el inodo de un archivo
ls -i /etc/hosts
# 1042 /etc/hosts

# Ver uso de inodos del filesystem
df -i
# Filesystem     Inodes   IUsed  IFree IUse% Mounted on
# /dev/vda3     1245184  125432 1119752   11% /

# Ver detalles del inodo con stat
stat /etc/hosts
# File: /etc/hosts
# Size: 221        Blocks: 8    IO Block: 4096   regular file
# Device: 252,3    Inode: 1042    Links: 1
# Access: (0644/-rw-r--r--)  Uid: (0/root)  Gid: (0/root)
# Access: 2026-03-19 10:30:00
# Modify: 2026-03-18 15:00:00
# Change: 2026-03-18 15:00:00
```

### Bloques de datos

Los datos reales del archivo se almacenan en **bloques** — típicamente de 4096 bytes (4 KiB). Un archivo de 10 KiB ocupa 3 bloques (el tercero se usa parcialmente):

```
 Archivo de 10 KiB
 Inodo → [blk 500][blk 501][blk 502]

 Bloque 500          Bloque 501          Bloque 502
┌───────────────┐   ┌───────────────┐   ┌───────────────┐
│ 4096 bytes    │   │ 4096 bytes    │   │ 1808 bytes    │
│ de datos      │   │ de datos      │   │ datos + pad   │
│               │   │               │   │ (2288 bytes   │
│               │   │               │   │  desperdicio) │
└───────────────┘   └───────────────┘   └───────────────┘
```

Los bloques no necesitan ser contiguos en el disco — el inodo tiene punteros a cada bloque. Cuando los bloques están dispersos, se llama **fragmentación**.

### Cómo se conecta todo: resolver un path

Cuando el kernel necesita abrir `/etc/hosts`:

```
1. Leer inodo #2 (/ es siempre inodo 2)
   → directorio raíz, leer sus bloques de datos
   → buscar entrada "etc" → inodo #15

2. Leer inodo #15 (/etc)
   → directorio, leer sus bloques de datos
   → buscar entrada "hosts" → inodo #1042

3. Leer inodo #1042 (/etc/hosts)
   → archivo regular, punteros a bloques 500, 501, 502
   → leer esos bloques → contenido del archivo
```

Cada componente del path requiere leer al menos un inodo y sus bloques — por eso los paths muy profundos son más lentos.

---

## 3. Journaling: protección contra corrupción

¿Qué pasa si se corta la luz mientras el kernel está escribiendo un archivo? Sin protección, los metadatos (inodo, directorio) pueden quedar inconsistentes con los bloques de datos — archivo corrupto, directorio roto, bloques huérfanos.

### El problema

```
 Escribir archivo nuevo: 3 operaciones
 1. Asignar bloques de datos        ← completada
 2. Crear entrada en directorio     ← ¡CORTE DE LUZ AQUÍ!
 3. Actualizar inodo con punteros   ← no ejecutada

 Resultado: bloques asignados pero inodo no actualiza → bloques perdidos
```

### La solución: journal

El journal (diario) es una zona reservada del disco donde el filesystem escribe **qué va a hacer ANTES de hacerlo**. Si se interrumpe la operación, al reiniciar puede:

- Si la entrada del journal está completa → repetir la operación (replay).
- Si está incompleta → descartar (la operación nunca llegó al disco).

```
 Sin journal:         Con journal:
 Escritura directa    1. Escribir intención al journal
      │                  2. Ejecutar la operación
      ▼                  3. Marcar journal entry como completa
 ¿Corte de luz?
 → corrupción        ¿Corte de luz en paso 1?
   posible            → journal incompleto → descartar
                     ¿Corte de luz en paso 2?
                      → journal completo → replay al reiniciar
```

### Modos de journaling (ext4)

ext4 ofrece tres modos que balancean seguridad vs rendimiento:

| Modo | Qué se journalea | Rendimiento | Seguridad |
|------|-------------------|:-----------:|:---------:|
| `journal` | Metadatos Y datos | Lento | Máxima |
| `ordered` (default) | Solo metadatos, pero datos se escriben antes | Medio | Alta |
| `writeback` | Solo metadatos, datos en cualquier orden | Rápido | Menor |

En modo **ordered** (el default), el filesystem garantiza que los datos se escriben al disco antes que los metadatos que los referencian. Así, si falla, puedes perder la última escritura pero el filesystem queda consistente.

```bash
# Ver qué modo de journal usa tu filesystem ext4
sudo tune2fs -l /dev/vda2 | grep "Journal features"
# Journal features: journal_checksum_v3

# El modo se configura en las opciones de montaje
mount | grep /boot
# /dev/vda2 on /boot type ext4 (rw,relatime,seclabel)
# Si no dice data=journal ni data=writeback, es "ordered" (default)
```

### Journaling en XFS y Btrfs

- **XFS**: journal de metadatos siempre activo, altamente optimizado para escritura paralela.
- **Btrfs**: no necesita journal tradicional — usa **copy-on-write** (nunca sobrescribe datos existentes, siempre escribe en bloques nuevos), lo que da integridad por diseño.

---

## 4. ext4: el estándar de facto

ext4 (fourth extended filesystem) es la evolución de ext2 → ext3 → ext4. Es el filesystem por defecto en Debian, Ubuntu y la mayoría de distribuciones. Fiable, maduro y bien documentado.

### Características principales

| Característica | Valor |
|---------------|-------|
| Tamaño máx. de archivo | 16 TiB |
| Tamaño máx. de filesystem | 1 EiB |
| Tamaño de bloque | 1–64 KiB (4 KiB por defecto) |
| Journaling | Sí (metadata ordered por defecto) |
| Extents | Sí (reemplazan punteros indirectos — más eficiente para archivos grandes) |
| Redimensionar en caliente | Crecer sí, reducir solo offline |
| fsck offline | Sí |
| Timestamps | Nanosegundos, hasta año 2446 |

### Herramientas de ext4

```bash
# Crear filesystem ext4
sudo mkfs.ext4 /dev/vdb1

# Ver información del superbloque
sudo tune2fs -l /dev/vdb1

# Cambiar label
sudo tune2fs -L "DATOS" /dev/vdb1

# Cambiar el porcentaje reservado para root (default 5%)
sudo tune2fs -m 1 /dev/vdb1

# Verificar/reparar (SOLO con la partición desmontada)
sudo fsck.ext4 /dev/vdb1

# Redimensionar para usar todo el espacio disponible
sudo resize2fs /dev/vdb1

# Ver fragmentación de un archivo
filefrag /ruta/al/archivo
```

### El 5% reservado para root

Por defecto, ext4 reserva el 5% del espacio para el usuario root. En un disco de 100 GiB, eso son 5 GiB que los usuarios normales no pueden usar. El propósito:

- Evitar que el sistema se quede sin espacio (root puede seguir operando).
- Permitir que procesos críticos escriban logs aunque `/` esté "lleno".

Para discos de **datos** (no el sistema), el 5% es desperdicio. Redúcelo:

```bash
# Reducir a 1% (razonable para discos de datos)
sudo tune2fs -m 1 /dev/vdb1

# Eliminar completamente (solo para datos, NUNCA para /)
sudo tune2fs -m 0 /dev/vdb1
```

---

## 5. XFS: alto rendimiento

XFS fue creado por SGI para servidores de alto rendimiento. Es el filesystem por defecto en **RHEL, Fedora y derivados** desde RHEL 7.

### Características principales

| Característica | Valor |
|---------------|-------|
| Tamaño máx. de archivo | 8 EiB |
| Tamaño máx. de filesystem | 8 EiB |
| Journaling | Sí (metadatos) |
| Allocation groups | Sí (permite I/O paralelo) |
| Redimensionar en caliente | Solo crecer, **no se puede reducir** |
| Defragmentación online | Sí (`xfs_fsr`) |
| Reflink (copy-on-write) | Sí (desde Linux 4.9, default en RHEL 8+) |

### Allocation groups: la ventaja de XFS

XFS divide el filesystem en **allocation groups** (AG) independientes, cada uno con su propio superbloque, tabla de inodos y espacio libre. Esto permite que múltiples operaciones de I/O ocurran en paralelo sin contención:

```
 ext4: un solo conjunto de estructuras
┌──────────────────────────────────────┐
│ [superblock][inode table][data....]  │ ← todo serializado
└──────────────────────────────────────┘

 XFS: múltiples allocation groups
┌──────────┬──────────┬──────────┬──────────┐
│   AG 0   │   AG 1   │   AG 2   │   AG 3   │
│ [sb][it] │ [sb][it] │ [sb][it] │ [sb][it] │
│ [data..] │ [data..] │ [data..] │ [data..] │
└──────────┴──────────┴──────────┴──────────┘
  ↑ cada AG opera independientemente = I/O paralelo
```

### Herramientas de XFS

```bash
# Crear filesystem XFS
sudo mkfs.xfs /dev/vdb1

# Ver información del superbloque
sudo xfs_info /dev/vdb1          # o xfs_info /mountpoint

# Cambiar label
sudo xfs_admin -L "DATOS" /dev/vdb1

# Verificar/reparar (SOLO desmontado)
sudo xfs_repair /dev/vdb1

# Crecer para usar todo el espacio (montado)
sudo xfs_growfs /mountpoint      # nota: usa el mountpoint, no el device

# Defragmentar online
sudo xfs_fsr /mountpoint
```

### La limitación crítica: no se puede reducir

```bash
# ext4: puedes reducir
sudo resize2fs /dev/vdb1 5G      # ✓ funciona

# XFS: NO puedes reducir
sudo xfs_growfs /data             # ✓ crecer sí
# ¿reducir? No existe el comando. No se puede. Punto.
```

Si necesitas reducir un filesystem XFS, la única opción es:
1. Hacer backup de los datos.
2. Recrear la partición más pequeña.
3. Crear nuevo XFS.
4. Restaurar los datos.

---

## 6. Btrfs: copy-on-write y snapshots nativos

Btrfs (B-tree filesystem, pronunciado "butter-FS") es el filesystem de nueva generación. Default en **openSUSE** y **Fedora Workstation** (desde Fedora 33).

### Características principales

| Característica | Valor |
|---------------|-------|
| Tamaño máx. de filesystem | 16 EiB |
| Copy-on-write (CoW) | Sí (inherente al diseño) |
| Snapshots | Sí (instantáneos, casi sin costo) |
| Compresión transparente | Sí (zlib, zstd, lzo) |
| Checksums de datos | Sí (CRC32C por defecto) |
| Subvolúmenes | Sí (como particiones lógicas sin reparticionar) |
| RAID integrado | Sí (RAID 0, 1, 10; RAID 5/6 inestable) |
| Redimensionar | Crecer y reducir online |

### Copy-on-write: cómo funciona

En ext4/XFS, modificar un archivo sobreescribe los bloques existentes. En Btrfs, **nunca se sobreescriben datos existentes** — los nuevos datos se escriben en bloques nuevos y luego se actualiza el puntero:

```
 ext4 (overwrite in place):
 Bloque 500: [datos viejos] → [datos nuevos] (sobreescrito)
 Si falla a medio camino → bloque corrupto

 Btrfs (copy-on-write):
 Bloque 500: [datos viejos]  (intacto)
 Bloque 700: [datos nuevos]  (escrito en bloque nuevo)
 Puntero: 500 → 700          (atómico: o cambia o no)
 Si falla antes del puntero → datos viejos intactos
 Bloque 500 se libera después
```

Esto da **integridad de datos por diseño** — no necesita journal tradicional.

### Snapshots: la killer feature

Un snapshot en Btrfs es instantáneo y no copia datos — solo congela los punteros actuales. Los datos nuevos se escriben en bloques nuevos (CoW), así que el snapshot y el filesystem activo comparten los bloques sin modificar:

```bash
# Crear un snapshot de un subvolumen
sudo btrfs subvolume snapshot / /snapshots/root-backup

# Crear snapshot de solo lectura (útil para backups)
sudo btrfs subvolume snapshot -r / /snapshots/root-readonly

# Listar subvolúmenes y snapshots
sudo btrfs subvolume list /

# Borrar un snapshot
sudo btrfs subvolume delete /snapshots/root-backup
```

### Por qué importa para VMs

Btrfs en el host permite hacer snapshots de las imágenes de disco de las VMs (que están en `/var/lib/libvirt/images/`) sin copiar los archivos — es prácticamente instantáneo:

```bash
# Si /var/lib/libvirt/images/ está en Btrfs
sudo btrfs subvolume snapshot /var/lib/libvirt/images \
  /var/lib/libvirt/images-snap-20260319
# Instantáneo, sin importar si las imágenes pesan 50 GiB
```

### Herramientas de Btrfs

```bash
# Crear filesystem Btrfs
sudo mkfs.btrfs /dev/vdb1

# Ver información
sudo btrfs filesystem show /dev/vdb1

# Ver uso de espacio (más preciso que df)
sudo btrfs filesystem df /mountpoint

# Activar compresión transparente
sudo mount -o compress=zstd /dev/vdb1 /data

# Verificar checksums (online)
sudo btrfs scrub start /mountpoint
sudo btrfs scrub status /mountpoint
```

---

## 7. Otros filesystems relevantes

### FAT32 / VFAT

```bash
sudo mkfs.vfat -F 32 /dev/vdb1
```

- Sin permisos Unix, sin journaling, sin archivos >4 GiB.
- **Uso**: partición EFI (ESP), intercambio de datos con Windows, USB drives.
- La partición ESP de UEFI **debe** ser FAT32.

### swap

No es un filesystem propiamente — es espacio de intercambio que el kernel usa como extensión de la RAM:

```bash
sudo mkswap /dev/vdb2
sudo swapon /dev/vdb2
```

- No se monta con `mount` — se activa con `swapon`.
- No contiene archivos ni directorios.
- En fstab usa tipo `swap`.

### tmpfs

Filesystem en RAM, no en disco. Se pierde al reiniciar:

```bash
mount | grep tmpfs
# tmpfs on /tmp type tmpfs (rw,nosuid,nodev,size=8G)
```

- Rapidísimo (es RAM).
- Útil para datos temporales.
- Ya lo viste en C01 (sistemas de archivos virtuales).

---

## 8. mkfs: crear un filesystem

`mkfs` (make filesystem) es un front-end que invoca la herramienta específica de cada tipo:

```bash
mkfs.ext4  → /sbin/mkfs.ext4  (o mke2fs -t ext4)
mkfs.xfs   → /sbin/mkfs.xfs
mkfs.btrfs → /sbin/mkfs.btrfs
mkfs.vfat  → /sbin/mkfs.vfat
```

### Uso básico

```bash
# ext4 con defaults (lo más común)
sudo mkfs.ext4 /dev/vdb1

# XFS
sudo mkfs.xfs /dev/vdb1

# Btrfs
sudo mkfs.btrfs /dev/vdb1

# FAT32
sudo mkfs.vfat -F 32 /dev/vdb1
```

### Opciones útiles de mkfs.ext4

```bash
# Con label personalizado
sudo mkfs.ext4 -L "DATOS" /dev/vdb1

# Con menos espacio reservado para root (1% en vez de 5%)
sudo mkfs.ext4 -m 1 /dev/vdb1

# Con más inodos (para muchos archivos pequeños)
sudo mkfs.ext4 -i 4096 /dev/vdb1
# -i = bytes por inodo; menor número = más inodos

# Con bloque de 1 KiB (para particiones muy pequeñas)
sudo mkfs.ext4 -b 1024 /dev/vdb1

# Combinar opciones
sudo mkfs.ext4 -L "DATOS" -m 1 /dev/vdb1
```

### Opciones útiles de mkfs.xfs

```bash
# Con label
sudo mkfs.xfs -L "DATOS" /dev/vdb1

# Forzar (si ya tiene un filesystem)
sudo mkfs.xfs -f /dev/vdb1

# Con tamaño de bloque específico (default 4096)
sudo mkfs.xfs -b size=4096 /dev/vdb1
```

### Qué pasa internamente al ejecutar mkfs

```
sudo mkfs.ext4 /dev/vdb1

1. Escribe el superbloque (información global del filesystem)
2. Crea la tabla de inodos (preasigna espacio para N inodos)
3. Crea copias de seguridad del superbloque
4. Reserva espacio para el journal
5. Crea el directorio raíz (inodo #2)
6. Genera UUID aleatorio
7. Escribe el lost+found directory
```

Después de mkfs, `blkid` mostrará el nuevo filesystem:

```bash
sudo blkid /dev/vdb1
# /dev/vdb1: UUID="nuevo-uuid-generado" TYPE="ext4" ...
```

### Formatear es destructivo

`mkfs` **destruye todo** lo que había en la partición. No pide confirmación (excepto `mkfs.xfs` si detecta un filesystem existente, que pide `-f`). Siempre verifica con `lsblk` y `blkid` que estás formateando la partición correcta:

```bash
# ANTES de mkfs, siempre verificar
lsblk
sudo blkid /dev/vdb1   # ¿tiene datos?
mount | grep vdb1       # ¿está montada?

# Solo entonces
sudo mkfs.ext4 /dev/vdb1
```

---

## 9. fsck: verificar y reparar

`fsck` (filesystem check) examina la estructura interna del filesystem buscando inconsistencias y las repara.

### Regla de oro: NUNCA ejecutar fsck en un filesystem montado

```bash
# ✗ PELIGROSO — puede destruir datos
sudo fsck.ext4 /dev/vda3    # si / está montada aquí → desastre

# ✓ Correcto — primero desmontar
sudo umount /dev/vdb1
sudo fsck.ext4 /dev/vdb1
```

Ejecutar fsck en un filesystem montado es como operar a un paciente que está corriendo — las estructuras de datos cambian mientras fsck las lee, causando corrupción.

### Uso básico

```bash
# Verificar (preguntar antes de reparar)
sudo fsck.ext4 /dev/vdb1

# Reparar automáticamente (responder "yes" a todo)
sudo fsck.ext4 -y /dev/vdb1

# Solo verificar, no reparar (dry run)
sudo fsck.ext4 -n /dev/vdb1

# Forzar verificación aunque esté marcado como clean
sudo fsck.ext4 -f /dev/vdb1
```

### fsck para XFS

```bash
# XFS usa xfs_repair en vez de fsck.xfs
sudo xfs_repair /dev/vdb1

# Dry run (solo verificar)
sudo xfs_repair -n /dev/vdb1
```

`fsck.xfs` existe pero no hace nada útil — es un wrapper que recomienda usar `xfs_repair`.

### fsck para Btrfs

```bash
# Btrfs tiene verificación online con scrub (no necesita desmontar)
sudo btrfs scrub start /mountpoint

# Para reparación seria (offline)
sudo btrfs check /dev/vdb1

# Reparar (último recurso)
sudo btrfs check --repair /dev/vdb1
```

### Cuándo se ejecuta fsck

- **Al boot**: si el filesystem no se desmontó limpiamente (corte de luz), el campo `pass` en fstab determina si fsck corre automáticamente.
- **Manualmente**: cuando sospechas corrupción.
- **Periódicamente**: ext4 puede configurarse para forzar fsck cada N montajes o N días.

### El directorio lost+found

Cuando fsck encuentra archivos cuyos directorios padre se corrompieron (bloques de datos sin inodo que los referencie), los coloca en `lost+found/` con nombres basados en su número de inodo:

```bash
ls /lost+found/
# #12345  #12346  #12347
# (archivos recuperados, sin nombre original)
```

Solo ext2/3/4 tiene `lost+found`. XFS y Btrfs usan otros mecanismos de recuperación.

---

## 10. Comparación y criterios de elección

### Tabla comparativa

| Criterio | ext4 | XFS | Btrfs |
|----------|:----:|:---:|:-----:|
| Madurez/estabilidad | Excelente | Excelente | Buena (mejorando) |
| Rendimiento general | Bueno | Muy bueno | Bueno |
| Archivos grandes | Bueno | Excelente | Bueno |
| Muchos archivos pequeños | Bueno | Medio | Bueno |
| Crecer online | Sí | Sí | Sí |
| Reducir | Offline | No | Online |
| Snapshots | No | No | Sí (nativo) |
| Compresión | No | No | Sí (zstd) |
| Checksums de datos | No | No | Sí |
| RAID integrado | No | No | Sí |
| Herramientas de repair | Maduras | Maduras | Mejorando |
| Default en | Debian, Ubuntu | RHEL, Fedora server | openSUSE, Fedora WS |

### Criterio de elección rápido

```
 ¿Qué filesystem usar?

 ¿Necesitas snapshots o compresión?
     Sí → Btrfs
     No ↓

 ¿Es RHEL/Fedora server o archivos muy grandes?
     Sí → XFS
     No ↓

 ¿Necesitas poder reducir el filesystem?
     Sí → ext4
     No ↓

 ¿Cuál es el default de tu distro?
     → Usa ese (ext4 o XFS probablemente)
```

### Para VMs en este curso

- **Disco del sistema (/)**: lo que el instalador elija (ext4 en Debian, XFS en Fedora).
- **Discos de datos adicionales**: ext4 es la opción más segura para aprender — más documentación, más herramientas, comportamiento predecible. XFS es igualmente válido si usas Fedora.

---

## 11. Errores comunes

### Error 1: ejecutar mkfs en la partición equivocada

```bash
sudo mkfs.ext4 /dev/vda3
# ¡Acabas de destruir tu partición raíz!
```

**Por qué pasa**: un typo, o confundir `/dev/vda` con `/dev/vdb`. mkfs no pregunta "¿estás seguro?" en ext4.

**Solución**: verificar siempre con `lsblk` y `mount` antes de formatear:

```bash
lsblk -f            # ver qué tiene cada partición
mount | grep vdb     # confirmar que no está montada
sudo mkfs.ext4 /dev/vdb1   # ahora sí
```

### Error 2: quedarse sin inodos con espacio libre

```bash
df -h /data
# Filesystem  Size  Used  Avail  Use%
# /dev/vdb1   10G   2G    8G     20%   ← hay espacio

touch /data/newfile
# No space left on device    ← ¿¿??

df -i /data
# Filesystem  Inodes  IUsed    IFree  IUse%
# /dev/vdb1   655360  655360   0      100%  ← ¡inodos agotados!
```

**Por qué pasa**: millones de archivos pequeños agotan los inodos antes que el espacio. Cada archivo (por pequeño que sea) usa un inodo.

**Solución**: al crear el filesystem, ajustar el ratio bytes/inodo:

```bash
# Más inodos (para muchos archivos pequeños, ej: mail server)
sudo mkfs.ext4 -i 4096 /dev/vdb1
# Un inodo por cada 4 KiB → muchos más inodos

# Default es -i 16384 (un inodo por cada 16 KiB)
```

En XFS, los inodos se asignan dinámicamente — este problema es menos común.

### Error 3: usar fsck en un filesystem montado

```bash
sudo fsck.ext4 /dev/vda3   # / está montada aquí
# ¡Puede destruir el filesystem!
```

**Por qué pasa**: fsck modifica estructuras de datos que el kernel también está modificando simultáneamente.

**Solución**: siempre desmontar primero. Para el filesystem raíz, arrancar desde un live USB o usar el modo de emergencia:

```bash
# Para particiones que no son /
sudo umount /dev/vdb1
sudo fsck.ext4 -f /dev/vdb1
sudo mount /dev/vdb1 /data
```

### Error 4: mkfs.xfs rechaza crear filesystem

```bash
sudo mkfs.xfs /dev/vdb1
# mkfs.xfs: /dev/vdb1 appears to contain an existing filesystem (ext4).
# Use the -f option to force overwrite.
```

**Por qué pasa**: XFS detecta firmas de un filesystem existente y se protege.

**Solución**: si estás seguro de que quieres sobreescribir:

```bash
sudo mkfs.xfs -f /dev/vdb1
```

ext4 no tiene esta protección — otro motivo para verificar con `blkid` antes.

### Error 5: confundir resize2fs con xfs_growfs

```bash
# Intentar redimensionar XFS con herramienta de ext4
sudo resize2fs /dev/vdb1
# resize2fs: Bad magic number in super-block

# Intentar redimensionar ext4 con herramienta de XFS
sudo xfs_growfs /data
# xfs_growfs: /data is not a valid XFS filesystem
```

**Por qué pasa**: cada filesystem tiene su propia herramienta de redimensionamiento.

**Solución**: verificar el tipo primero:

```bash
lsblk -f /dev/vdb1   # ver FSTYPE
# ext4  → resize2fs /dev/vdb1
# xfs   → xfs_growfs /mountpoint   (nota: xfs usa el mountpoint, no el device)
# btrfs → btrfs filesystem resize max /mountpoint
```

---

## 12. Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│              SISTEMAS DE ARCHIVOS — REFERENCIA                   │
├──────────────────────────────────────────────────────────────────┤
│ CREAR FILESYSTEM                                                 │
│   mkfs.ext4 /dev/vdb1            ext4 (default Debian/Ubuntu)   │
│   mkfs.ext4 -L "LABEL" -m 1     Con label, 1% reservado        │
│   mkfs.xfs /dev/vdb1             XFS (default RHEL/Fedora)      │
│   mkfs.xfs -f /dev/vdb1          XFS forzar sobreescritura      │
│   mkfs.btrfs /dev/vdb1           Btrfs                          │
│   mkfs.vfat -F 32 /dev/vdb1      FAT32 (ESP, USB)              │
│   mkswap /dev/vdb2               Swap                           │
├──────────────────────────────────────────────────────────────────┤
│ INSPECCIONAR                                                     │
│   lsblk -f                       Tipo, UUID, mountpoint         │
│   blkid /dev/vdb1                UUID, TYPE, PARTUUID           │
│   df -Th                         Espacio usado por filesystem   │
│   df -i                          Uso de inodos                  │
│   stat /ruta/archivo             Inodo, permisos, timestamps    │
├──────────────────────────────────────────────────────────────────┤
│ ext4                                                             │
│   tune2fs -l /dev/vdb1           Info del superbloque           │
│   tune2fs -L "LABEL" /dev/vdb1   Cambiar label                 │
│   tune2fs -m 1 /dev/vdb1         Cambiar % reservado            │
│   resize2fs /dev/vdb1            Redimensionar (crecer/reducir) │
│   fsck.ext4 -f /dev/vdb1         Verificar (¡desmontado!)      │
├──────────────────────────────────────────────────────────────────┤
│ XFS                                                              │
│   xfs_info /dev/vdb1             Info del superbloque           │
│   xfs_admin -L "LABEL" /dev/vdb1 Cambiar label                 │
│   xfs_growfs /mountpoint         Crecer (NO reducir)            │
│   xfs_repair /dev/vdb1           Verificar/reparar              │
│   xfs_fsr /mountpoint            Defragmentar online            │
├──────────────────────────────────────────────────────────────────┤
│ Btrfs                                                            │
│   btrfs filesystem show          Info del filesystem            │
│   btrfs filesystem df /mnt       Uso de espacio (preciso)       │
│   btrfs subvolume snapshot       Crear snapshot                 │
│   btrfs scrub start /mnt         Verificar checksums (online)   │
│   btrfs check /dev/vdb1          Verificar (offline)            │
├──────────────────────────────────────────────────────────────────┤
│ REGLAS CLAVE                                                     │
│   ✓ Verificar con lsblk antes de mkfs                           │
│   ✓ Desmontar antes de fsck                                     │
│   ✓ ext4: resize2fs DEVICE                                      │
│   ✓ XFS: xfs_growfs MOUNTPOINT (solo crecer)                    │
│   ✗ NUNCA fsck en filesystem montado                            │
│   ✗ NUNCA mkfs sin verificar qué partición es                   │
└──────────────────────────────────────────────────────────────────┘
```

---

## 13. Ejercicios

### Ejercicio 1: Crear y comparar ext4 y XFS

Usa las particiones que creaste en el ejercicio 3 de T02 (disco `/dev/vdb` con dos particiones), o crea un loop device con dos particiones:

1. Formatea la primera partición como ext4 y la segunda como XFS:
   ```bash
   sudo mkfs.ext4 -L "EXT4-TEST" /dev/vdb1
   sudo mkfs.xfs -L "XFS-TEST" /dev/vdb2
   ```
2. Verifica con `lsblk -f` que ambas tienen FSTYPE, UUID y LABEL.
3. Verifica con `sudo blkid` — ¿qué información adicional muestra?
4. Monta ambas:
   ```bash
   sudo mkdir -p /mnt/ext4-test /mnt/xfs-test
   sudo mount /dev/vdb1 /mnt/ext4-test
   sudo mount /dev/vdb2 /mnt/xfs-test
   ```
5. Compara la información interna:
   ```bash
   sudo tune2fs -l /dev/vdb1 | head -30     # ext4
   sudo xfs_info /dev/vdb2                   # XFS
   ```
6. Verifica el espacio con `df -Th`. ¿Cuánto espacio está "usado" en cada uno recién formateado? ¿Por qué no es 0?
7. Verifica los inodos con `df -i`. ¿Cuál tiene más inodos preasignados?

> **Pregunta de reflexión**: el espacio "usado" en un filesystem recién creado corresponde a las estructuras internas (superbloque, tabla de inodos, journal). ¿Cuál de los dos ocupa más espacio en overhead? ¿Cambia esa proporción con discos más grandes?

### Ejercicio 2: Inodos y el 5% reservado

Sobre la partición ext4 del ejercicio anterior:

1. Revisa cuántos inodos tiene con `df -i`.
2. Revisa el porcentaje reservado para root:
   ```bash
   sudo tune2fs -l /dev/vdb1 | grep "Reserved block count"
   ```
3. Calcula cuánto espacio representa ese 5% reservado.
4. Cambia el reservado a 1%:
   ```bash
   sudo tune2fs -m 1 /dev/vdb1
   ```
5. Verifica con `df -h` que el espacio disponible aumentó.
6. Crea muchos archivos vacíos para experimentar:
   ```bash
   cd /mnt/ext4-test
   for i in $(seq 1 1000); do sudo touch "file_$i"; done
   ```
7. Revisa `df -i` de nuevo — ¿cuántos inodos se consumieron?

> **Pregunta de reflexión**: si tu caso de uso fuera un servidor de correo que almacena millones de emails como archivos individuales de pocos KiB, ¿qué opción de mkfs cambiarías al crear el filesystem? ¿Qué pasa si necesitas más inodos después de crear el filesystem en ext4?

### Ejercicio 3: Verificación y reparación

1. Desmonta la partición ext4:
   ```bash
   sudo umount /mnt/ext4-test
   ```
2. Ejecuta fsck en modo de solo lectura:
   ```bash
   sudo fsck.ext4 -n /dev/vdb1
   ```
   ¿Dice "clean"?
3. Fuerza una verificación completa:
   ```bash
   sudo fsck.ext4 -f /dev/vdb1
   ```
4. Ahora con la partición XFS:
   ```bash
   sudo umount /mnt/xfs-test
   sudo xfs_repair -n /dev/vdb2     # dry run
   ```
5. Monta de nuevo ambas y verifica que todo sigue funcionando.
6. Investiga: ¿qué contiene `/mnt/ext4-test/lost+found/`? ¿Por qué existe? ¿XFS tiene un equivalente?

> **Pregunta de reflexión**: ¿por qué fsck necesita que el filesystem esté desmontado pero `btrfs scrub` puede ejecutarse online? ¿Qué propiedad de Btrfs lo hace posible?
