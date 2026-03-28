# Sistemas de Archivos

## Erratas corregidas respecto al archivo fuente

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| `README.md` | 241 | "Tamaño de bloque: 1–64 KiB" — 64 KiB solo es posible en arquitecturas con páginas de 64K (ciertos ARM64). En x86_64 (las VMs de este curso), el máximo es 4 KiB | Tamaño de bloque: 1–4 KiB en x86_64 (4 KiB por defecto) |

---

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
- El **número de inodos se fija al crear el filesystem** (en ext4). Si se agotan, no puedes crear más archivos aunque haya espacio libre en disco.

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

 Resultado: bloques asignados pero inodo no actualizado → bloques perdidos
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
| Tamaño de bloque | 1–4 KiB en x86_64 (4 KiB por defecto) |
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
 ext4: block groups (algo de paralelismo, pero más serializado)
┌──────────────────────────────────────┐
│ [superblock][inode table][data....]  │
└──────────────────────────────────────┘

 XFS: múltiples allocation groups (independientes)
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
  /var/lib/libvirt/images-snap-20260326
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

Cuando fsck encuentra archivos cuyos directorios padre se corrompieron (datos con inodo intacto pero sin entrada de directorio que apunte a ellos), los coloca en `lost+found/` con nombres basados en su número de inodo:

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

### Ejercicio 1: Crear ext4 y XFS en un loop device

Crea un loop device con dos particiones y formatea cada una con un filesystem distinto:

```bash
dd if=/dev/zero of=/tmp/fs-practice.img bs=1M count=512
sudo losetup -fP /tmp/fs-practice.img
# Anotar el loop device (ej: /dev/loop0)

sudo fdisk /dev/loop0 <<'EOF'
g
n
1

+256M
n
2


w
EOF
sudo partprobe /dev/loop0

# Formatear
sudo mkfs.ext4 -L "EXT4-TEST" /dev/loop0p1
sudo mkfs.xfs -L "XFS-TEST" /dev/loop0p2
```

Después verifica con `lsblk -f` y `sudo blkid`.

**Predicción antes de ejecutar:**

<details><summary>¿Qué columnas mostrará lsblk -f para cada partición? ¿blkid mostrará algo diferente?</summary>

`lsblk -f` mostrará: NAME, FSTYPE (`ext4`/`xfs`), FSVER, LABEL (`EXT4-TEST`/`XFS-TEST`), UUID y MOUNTPOINTS (vacío porque aún no están montadas).

`blkid` mostrará adicionalmente: BLOCK_SIZE, PARTUUID (el UUID de la partición GPT, distinto del UUID del filesystem), y posiblemente PARTLABEL.

La diferencia clave: `blkid` lee directamente las firmas del disco, mientras `lsblk -f` lee del cache de udev/sysfs.
</details>

---

### Ejercicio 2: Explorar el superbloque de ext4

Examina las estructuras internas del filesystem ext4 creado:

```bash
# Montar e inspeccionar
sudo mkdir -p /mnt/ext4-test
sudo mount /dev/loop0p1 /mnt/ext4-test

# Superbloque
sudo tune2fs -l /dev/loop0p1 | head -30

# Copias de seguridad del superbloque
sudo dumpe2fs /dev/loop0p1 2>/dev/null | grep -i "superblock"

# Uso de espacio e inodos
df -Th /mnt/ext4-test
df -i /mnt/ext4-test
```

**Predicción antes de ejecutar:**

<details><summary>En un ext4 de 256 MiB recién creado, ¿cuánto espacio aparecerá como "Used" en df? ¿Por qué no es 0?</summary>

Aparecerá aproximadamente **12-14 MiB** como usado, a pesar de no tener archivos de usuario. Este espacio corresponde a:
- El superbloque y sus copias de backup
- La tabla de inodos preasignada (espacio reservado para ~65000 inodos)
- El journal (ocupa ~16-32 MiB en un filesystem pequeño, o ~128 MiB en uno grande)
- El directorio `lost+found`

Además, el 5% del espacio está reservado para root, así que `Avail` será menor que `Size - Used`. El filesystem tiene overhead estructural inevitable.
</details>

---

### Ejercicio 3: Inodos y el agotamiento

Experimenta con la creación masiva de archivos para entender los inodos:

```bash
# Ver cuántos inodos tiene el filesystem
df -i /mnt/ext4-test

# Crear 1000 archivos vacíos
cd /mnt/ext4-test
for i in $(seq 1 1000); do sudo touch "file_$i"; done

# Ver cuántos inodos se consumieron
df -i /mnt/ext4-test

# Ver el inodo de un archivo específico
ls -i file_500
stat file_500
```

**Predicción antes de ejecutar:**

<details><summary>Cada archivo vacío, ¿ocupa espacio en disco? ¿Cuántos inodos consume crear 1000 archivos?</summary>

- Un archivo vacío ocupa **0 bytes de datos** pero sí ocupa **un inodo** y una **entrada de directorio**. Con `df -h` no verás aumento significativo de espacio, pero con `df -i` verás ~1000 inodos más usados.
- Los 1000 archivos consumen exactamente **1000 inodos** (uno por archivo).
- El directorio `/mnt/ext4-test/` también crece porque necesita almacenar 1000 entradas de directorio (nombre → inodo) en sus bloques de datos. Esto puede ocupar unos pocos bloques adicionales (4-8 KiB).

Limpieza: `cd / && sudo rm -f /mnt/ext4-test/file_*`
</details>

---

### Ejercicio 4: El 5% reservado para root

Examina y modifica el espacio reservado en ext4:

```bash
# Ver porcentaje reservado actual
sudo tune2fs -l /dev/loop0p1 | grep -i reserved

# Ver efecto en el espacio disponible
df -h /mnt/ext4-test

# Reducir a 1%
sudo tune2fs -m 1 /dev/loop0p1

# Verificar que Avail aumentó
df -h /mnt/ext4-test

# Reducir a 0% (solo para experimentar)
sudo tune2fs -m 0 /dev/loop0p1
df -h /mnt/ext4-test
```

**Predicción antes de ejecutar:**

<details><summary>En un filesystem de 256 MiB, ¿cuántos MiB recuperas al pasar de 5% a 0% reservado?</summary>

Recuperas aproximadamente **12-13 MiB** (5% de 256 MiB). La columna `Avail` de `df -h` aumentará en esa cantidad. La columna `Used` no cambia — el espacio reservado no se "usa", sino que se oculta de los usuarios normales.

En discos de datos donde no hay procesos del sistema, reservar 0% es razonable. En particiones de sistema (`/`), mantener al menos 1-2% previene situaciones donde root no puede escribir logs o archivos temporales cuando el disco se llena.
</details>

---

### Ejercicio 5: Comparar ext4 y XFS — herramientas de inspección

Monta la partición XFS y compara las herramientas de cada filesystem:

```bash
sudo mkdir -p /mnt/xfs-test
sudo mount /dev/loop0p2 /mnt/xfs-test

# ext4: información del superbloque
sudo tune2fs -l /dev/loop0p1 | head -20

# XFS: información equivalente
sudo xfs_info /mnt/xfs-test

# Comparar uso de espacio
df -Th /mnt/ext4-test /mnt/xfs-test

# Comparar inodos
df -i /mnt/ext4-test /mnt/xfs-test
```

**Predicción antes de ejecutar:**

<details><summary>¿XFS tiene el mismo número de inodos preasignados que ext4? ¿Cuál tiene más overhead en un disco de 256 MiB?</summary>

- **No.** XFS asigna inodos **dinámicamente** — `df -i` mostrará muchos menos inodos usados inicialmente. A diferencia de ext4 (que preasigna la tabla de inodos completa al formatear), XFS crea inodos bajo demanda en cada allocation group.
- En un disco pequeño de 256 MiB, **ext4 tiene más overhead** proporcionalmente: la tabla de inodos preasignada y el journal consumen un porcentaje mayor del espacio total. En discos grandes, la diferencia se diluye.
- `xfs_info` muestra información estructural diferente a `tune2fs`: allocation groups, tamaño de bloque de datos vs bloque de log, sectores por AG, etc.
</details>

---

### Ejercicio 6: Verificar filesystem con fsck/xfs_repair

Practica la verificación (siempre desmontando primero):

```bash
# Desmontar ambas
sudo umount /mnt/ext4-test
sudo umount /mnt/xfs-test

# ext4: verificar
sudo fsck.ext4 -n /dev/loop0p1    # dry run
sudo fsck.ext4 -f /dev/loop0p1    # forzar verificación completa

# XFS: verificar
sudo xfs_repair -n /dev/loop0p2   # dry run

# Volver a montar
sudo mount /dev/loop0p1 /mnt/ext4-test
sudo mount /dev/loop0p2 /mnt/xfs-test
```

**Predicción antes de ejecutar:**

<details><summary>¿Qué dice fsck si el filesystem está clean? ¿Qué pasa si intentas fsck sin desmontar?</summary>

- En estado clean, `fsck.ext4 -n` mostrará algo como: `/dev/loop0p1: clean, X/Y files, Z/W blocks`. Con `-f` fuerza la verificación completa aunque esté clean y muestra el resumen de pasadas (pass 1-5).
- Si intentas `fsck.ext4` en una partición montada, mostrará una **advertencia**: `WARNING!!! The filesystem is mounted. If you continue you ***WILL*** cause ***SEVERE*** filesystem damage.` y preguntará si quieres continuar. Siempre responder **no**.
- `xfs_repair -n` muestra un resumen de las fases de verificación (1-7) y cualquier problema encontrado.
</details>

---

### Ejercicio 7: Modos de journaling de ext4

Examina el journal del ext4 y entiende los modos:

```bash
# Ver características del journal
sudo tune2fs -l /dev/loop0p1 | grep -i journal

# Ver el tamaño del journal
sudo dumpe2fs /dev/loop0p1 2>/dev/null | grep -i "journal size"

# Ver las opciones de montaje actuales
mount | grep loop0p1
# Si no dice data=, es "ordered" (default)
```

**Predicción antes de ejecutar:**

<details><summary>¿Cuánto ocupa el journal en un filesystem de 256 MiB? ¿Qué modo de journaling usa por defecto?</summary>

- El journal ocupa entre **8-16 MiB** en un filesystem pequeño de 256 MiB (proporcional al tamaño del filesystem, con un mínimo de ~1024 bloques).
- El modo por defecto es **ordered**: los metadatos se journalean, y los datos se escriben antes que los metadatos que los referencian (pero no se journalean). Esto da un buen balance entre seguridad e rendimiento.
- `tune2fs -l` mostrará `Journal features: journal_checksum_v3` (checksums en el journal para detectar corrupción).
- El journal es una zona reservada dentro del filesystem, no visible como archivo.
</details>

---

### Ejercicio 8: Crear filesystem con opciones personalizadas

Experimenta con diferentes opciones de mkfs:

```bash
# Desmontar y reformatear con opciones diferentes
sudo umount /mnt/ext4-test

# Más inodos (para muchos archivos pequeños)
sudo mkfs.ext4 -L "MANY-FILES" -m 1 -i 4096 /dev/loop0p1
sudo mount /dev/loop0p1 /mnt/ext4-test

# Comparar inodos con el default
df -i /mnt/ext4-test
# ¿Cuántos inodos hay ahora vs antes?

# Limpiar y reformatear con defaults para comparar
sudo umount /mnt/ext4-test
sudo mkfs.ext4 -L "DEFAULT" /dev/loop0p1
sudo mount /dev/loop0p1 /mnt/ext4-test
df -i /mnt/ext4-test
```

**Predicción antes de ejecutar:**

<details><summary>Con -i 4096, ¿cuántos inodos tendrá un filesystem de 256 MiB comparado con el default (-i 16384)?</summary>

- Con `-i 4096`: aproximadamente **65,536 inodos** (256 MiB / 4 KiB = 65,536). Esto significa un inodo por cada bloque de 4 KiB.
- Con `-i 16384` (default): aproximadamente **16,384 inodos** (256 MiB / 16 KiB). Un inodo por cada 4 bloques.
- La diferencia es **4x más inodos** con `-i 4096`, pero a cambio la tabla de inodos ocupa más espacio, dejando menos bloques de datos disponibles.
- Para un mail server con millones de archivos de pocos KiB, `-i 4096` o incluso `-i 2048` sería necesario. Para archivos grandes (videos, imágenes de disco), el default es más que suficiente.
</details>

---

### Ejercicio 9: Identificar filesystem de tu sistema

Analiza los filesystems de tu sistema real:

```bash
# Mapa completo
lsblk -f

# Espacio por filesystem
df -Th

# Uso de inodos
df -i

# Detalles del filesystem raíz
# Si es ext4:
sudo tune2fs -l /dev/vda3 2>/dev/null | head -20
# Si es XFS:
sudo xfs_info / 2>/dev/null
# Si es Btrfs:
sudo btrfs filesystem show / 2>/dev/null
```

**Predicción antes de ejecutar:**

<details><summary>Si tu sistema es Fedora Workstation, ¿qué filesystem tiene /? ¿Y /boot?</summary>

- En **Fedora Workstation** (33+), `/` usa **Btrfs** por defecto. Verás subvolúmenes como `root` y `home`.
- `/boot` usa **ext4** (Btrfs no es adecuado para /boot con GRUB en todas las configuraciones).
- La partición EFI (`/boot/efi`) usa **vfat** (FAT32) — requerido por la especificación UEFI.
- En **Debian**, `/` y `/boot` usan **ext4** por defecto.
- En **Fedora Server/RHEL**, `/` usa **XFS** por defecto.

Con `lsblk -f` verás la columna FSTYPE que confirma el tipo de cada partición montada.
</details>

---

### Ejercicio 10: Flujo completo en VM — de partición vacía a filesystem usable

Si tienes la VM con el disco `/dev/vdb` particionado (del ejercicio 10 de T02), completa el flujo:

```bash
# Dentro de la VM:
# 1. Verificar particiones existentes
lsblk /dev/vdb
sudo blkid /dev/vdb*

# 2. Formatear
sudo mkfs.ext4 -L "DATA" -m 1 /dev/vdb1
sudo mkfs.xfs -L "LOGS" /dev/vdb2

# 3. Crear puntos de montaje
sudo mkdir -p /data /logs

# 4. Montar
sudo mount /dev/vdb1 /data
sudo mount /dev/vdb2 /logs

# 5. Verificar
df -Th /data /logs
df -i /data /logs
lsblk -f /dev/vdb

# 6. Crear archivos de prueba
sudo touch /data/test-file
sudo touch /logs/test-log

# 7. Verificar thin provisioning desde el host
# (en el host):
qemu-img info /var/lib/libvirt/images/practice-disk.qcow2
```

**Predicción antes de ejecutar:**

<details><summary>Después de mkfs + crear dos archivos de prueba, ¿cuánto creció el archivo qcow2 en el host?</summary>

El qcow2 creció significativamente — de unos pocos KiB (solo tabla de particiones) a aproximadamente **30-50 MiB**, porque:
- `mkfs.ext4` escribe: superbloque, copias de backup, tabla de inodos, journal, `lost+found`. Esto toca muchos bloques a lo largo de toda la partición.
- `mkfs.xfs` escribe: superbloque de cada allocation group, journal, estructuras internas.
- Los dos archivos de prueba (`touch`) son insignificantes — ocupan un inodo y una entrada de directorio cada uno, no datos.

El crecimiento real del qcow2 lo puedes verificar comparando `disk size` en `qemu-img info` con el `virtual size` (que sigue siendo 2 GiB).

Limpieza del loop device: `sudo umount /mnt/ext4-test /mnt/xfs-test && sudo losetup -d /dev/loop0 && rm /tmp/fs-practice.img`
</details>
