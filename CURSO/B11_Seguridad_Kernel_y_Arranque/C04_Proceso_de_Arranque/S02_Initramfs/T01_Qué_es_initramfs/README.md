# Qué es initramfs

## Índice
1. [El problema que resuelve initramfs](#el-problema-que-resuelve-initramfs)
2. [initrd vs initramfs](#initrd-vs-initramfs)
3. [Cómo funciona initramfs](#cómo-funciona-initramfs)
4. [Contenido del initramfs](#contenido-del-initramfs)
5. [El init del initramfs](#el-init-del-initramfs)
6. [switch_root: la transición](#switch_root-la-transición)
7. [Cuándo se necesita regenerar](#cuándo-se-necesita-regenerar)
8. [Inspeccionar el initramfs](#inspeccionar-el-initramfs)
9. [Arranque sin initramfs](#arranque-sin-initramfs)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## El problema que resuelve initramfs

El kernel Linux puede compilar drivers directamente en el binario (`=y` en `.config`) o como módulos cargables (`=m`). Las distribuciones compilan la mayoría de drivers como módulos para mantener el kernel pequeño y genérico — un solo `vmlinuz` funciona en millones de configuraciones de hardware diferentes.

Pero esto crea un problema circular:

```
  ┌────────────────────────────────────────────────────────────┐
  │                                                            │
  │    Para montar /          Necesito drivers de disco         │
  │    (donde están          (ext4.ko, nvme.ko, sd_mod.ko)     │
  │     los módulos)                                           │
  │         │                          │                        │
  │         │         ┌────────┐       │                        │
  │         └────────▶│  ???   │◀──────┘                        │
  │                   └────────┘                                │
  │                                                            │
  │    Los módulos están en /lib/modules/                       │
  │    que está en el filesystem raíz                           │
  │    que está en el disco                                     │
  │    que necesita un driver                                   │
  │    que es un módulo                                         │
  │    que está en /lib/modules/...                             │
  │                                                            │
  └────────────────────────────────────────────────────────────┘

  SOLUCIÓN: initramfs
  ┌────────────────────────────────────────────────────────────┐
  │                                                            │
  │  GRUB carga initramfs en RAM junto con el kernel           │
  │                                                            │
  │  initramfs contiene:                                       │
  │  • Los módulos específicos para TU hardware                │
  │  • Herramientas para LVM, RAID, LUKS                       │
  │  • Un script /init que orquesta todo                       │
  │                                                            │
  │  Resultado:                                                │
  │  RAM(initramfs) → cargar módulos → acceder disco → montar /│
  │                                                            │
  └────────────────────────────────────────────────────────────┘
```

El initramfs no es solo para drivers de disco. También resuelve estos escenarios:

| Escenario | Sin initramfs | Con initramfs |
|---|---|---|
| **Root en LVM** | Kernel no sabe de LVM | initramfs ejecuta `vgchange -ay` |
| **Root en LUKS** | Kernel no puede descifrar | initramfs ejecuta `cryptsetup open` y pide passphrase |
| **Root en RAID** | Kernel no ensambla arrays | initramfs ejecuta `mdadm --assemble` |
| **Root por NFS** | Kernel necesita red + NFS | initramfs configura red y monta NFS |
| **Root en iSCSI** | Kernel necesita red + iSCSI | initramfs inicia sesión iSCSI |
| **Plymouth (splash)** | Sin pantalla gráfica de arranque | initramfs carga Plymouth |

---

## initrd vs initramfs

Históricamente existieron dos enfoques. Es importante distinguirlos porque la terminología se mezcla a menudo.

### initrd (obsoleto)

**initrd** (initial RAM disk) fue la solución original (Linux 2.4 y anterior):

```
  ┌────────────────────────────────────────────────────────┐
  │                    initrd (obsoleto)                    │
  │                                                        │
  │  1. Archivo de imagen de disco (ext2/ext3)             │
  │  2. Se carga en un ramdisk (/dev/ram0)                 │
  │  3. El kernel lo monta como un block device            │
  │  4. Usa caché de bloques (doble buffering)             │
  │  5. Tamaño fijo (reservado al crear la imagen)         │
  │  6. El script /linuxrc ejecuta las tareas              │
  │  7. pivot_root para cambiar al root real               │
  │                                                        │
  │  Problemas:                                            │
  │  • Doble buffering: datos en RAM + caché = desperdicio │
  │  • Tamaño fijo: desperdicias RAM o te quedas corto     │
  │  • Necesita driver de filesystem (ext2) built-in       │
  │  • pivot_root complejo (el viejo root queda montado)   │
  └────────────────────────────────────────────────────────┘
```

### initramfs (actual)

**initramfs** (initial RAM filesystem) reemplazó a initrd en Linux 2.6:

```
  ┌────────────────────────────────────────────────────────┐
  │                  initramfs (actual)                     │
  │                                                        │
  │  1. Archivo cpio comprimido (gzip/xz/zstd/lz4)        │
  │  2. Se extrae en tmpfs (RAM-based filesystem)          │
  │  3. NO es un block device, es un filesystem en RAM     │
  │  4. Sin doble buffering (tmpfs usa page cache directo) │
  │  5. Tamaño dinámico (crece/encoge según necesidad)     │
  │  6. El script /init ejecuta las tareas                 │
  │  7. switch_root limpio (libera RAM del initramfs)      │
  │                                                        │
  │  Ventajas:                                             │
  │  • Eficiente en RAM (sin desperdicio)                  │
  │  • No necesita driver de filesystem extra              │
  │  • tmpfs siempre disponible (built-in en el kernel)    │
  │  • switch_root libera toda la RAM del initramfs        │
  └────────────────────────────────────────────────────────┘
```

### Comparación

| Aspecto | initrd | initramfs |
|---|---|---|
| **Formato** | Imagen de filesystem (ext2) | Archivo cpio comprimido |
| **En RAM como** | Block device (/dev/ram0) | tmpfs |
| **Buffering** | Doble (ramdisk + page cache) | Simple (page cache) |
| **Tamaño** | Fijo | Dinámico |
| **Transición** | pivot_root | switch_root |
| **Script** | /linuxrc | /init |
| **Kernel req.** | ext2 built-in | tmpfs built-in (siempre) |
| **Estado** | Obsoleto | Actual |

> **Nota sobre nombres**: aunque initrd está obsoleto, el nombre persiste en muchos lugares: la directiva de GRUB sigue siendo `initrd`, el archivo en Debian se llama `/boot/initrd.img-*`, y mucha documentación dice "initrd" refiriéndose al initramfs. Internamente, todas las distribuciones modernas usan initramfs.

---

## Cómo funciona initramfs

### Formato: archivo cpio

El initramfs es un archivo **cpio** (Copy In/Copy Out) comprimido. cpio es un formato de archivo simple (como tar pero más primitivo) que el kernel puede descomprimir sin necesidad de herramientas externas.

```
  ┌──────────────────────────────────────────────────────┐
  │        /boot/initramfs-6.7.4.img                      │
  │                                                      │
  │  ┌────────────────────────────────────────────────┐  │
  │  │        Compresión (gzip/xz/zstd/lz4)           │  │
  │  │  ┌──────────────────────────────────────────┐  │  │
  │  │  │          Archivo cpio                     │  │  │
  │  │  │                                          │  │  │
  │  │  │  bin/sh                                  │  │  │
  │  │  │  bin/modprobe                            │  │  │
  │  │  │  sbin/cryptsetup                         │  │  │
  │  │  │  lib/modules/6.7.4/kernel/drivers/...    │  │  │
  │  │  │  etc/modprobe.d/...                      │  │  │
  │  │  │  init                                    │  │  │
  │  │  │  ...                                     │  │  │
  │  │  └──────────────────────────────────────────┘  │  │
  │  └────────────────────────────────────────────────┘  │
  └──────────────────────────────────────────────────────┘
```

En RHEL/Fedora con dracut, el initramfs puede tener un formato especial: un **cpio temprano** (sin comprimir, con microcode de CPU) seguido del cpio comprimido principal:

```
  ┌────────────────────────────────────────────────────────┐
  │  initramfs-6.7.4.img                                   │
  │                                                        │
  │  ┌──────────────────────┐  ┌────────────────────────┐  │
  │  │ Early cpio           │  │ Main cpio (compressed) │  │
  │  │ (sin comprimir)      │  │                        │  │
  │  │                      │  │ Módulos, herramientas, │  │
  │  │ CPU microcode:       │  │ /init, etc.            │  │
  │  │ kernel/x86/microcode │  │                        │  │
  │  │ /AuthenticAMD.bin    │  │                        │  │
  │  │ /GenuineIntel.bin    │  │                        │  │
  │  └──────────────────────┘  └────────────────────────┘  │
  └────────────────────────────────────────────────────────┘
```

El microcode de CPU se carga antes que cualquier otra cosa para parchear bugs del procesador (Spectre, Meltdown, etc.) lo antes posible.

### Ciclo de vida en memoria

```
  1. GRUB carga initramfs en RAM
     ┌─────────────────────────────────────────┐
     │ RAM                                      │
     │  ┌────────────┐  ┌───────────────────┐  │
     │  │  vmlinuz   │  │ initramfs.img     │  │
     │  │ (comprimido│  │ (comprimido)      │  │
     │  └────────────┘  └───────────────────┘  │
     └─────────────────────────────────────────┘

  2. Kernel se descomprime y toma control
     ┌─────────────────────────────────────────┐
     │ RAM                                      │
     │  ┌──────────────────┐  ┌─────────────┐  │
     │  │  Kernel (running) │  │ initramfs   │  │
     │  │  descomprimido    │  │ (aún cpio)  │  │
     │  └──────────────────┘  └─────────────┘  │
     └─────────────────────────────────────────┘

  3. Kernel crea tmpfs, extrae cpio en tmpfs, ejecuta /init
     ┌─────────────────────────────────────────┐
     │ RAM                                      │
     │  ┌──────────────────┐  ┌─────────────┐  │
     │  │  Kernel (running) │  │  tmpfs (/)  │  │
     │  │                  │  │  /init ←run  │  │
     │  │                  │  │  /bin/       │  │
     │  │                  │  │  /lib/       │  │
     │  │                  │  │  /sbin/      │  │
     │  └──────────────────┘  └─────────────┘  │
     └─────────────────────────────────────────┘

  4. /init monta root real, switch_root libera tmpfs
     ┌─────────────────────────────────────────┐
     │ RAM                                      │
     │  ┌──────────────────┐                    │
     │  │  Kernel (running) │   tmpfs liberado  │
     │  └──────────────────┘   (RAM devuelta)   │
     │                                          │
     │  / = disco real (ext4/xfs en /dev/sda3)  │
     │  /sbin/init = systemd (PID 1)            │
     └─────────────────────────────────────────┘
```

---

## Contenido del initramfs

### Módulos del kernel

El initramfs no incluye **todos** los módulos — solo los necesarios para el hardware del sistema donde se generó:

```bash
# List modules included in initramfs
lsinitrd /boot/initramfs-$(uname -r).img | grep "\.ko"

# Typical modules included:
# Storage drivers
#   kernel/drivers/ata/ahci.ko          (SATA)
#   kernel/drivers/nvme/host/nvme.ko     (NVMe)
#   kernel/drivers/scsi/sd_mod.ko        (SCSI disk)
#   kernel/drivers/block/virtio_blk.ko   (VM virtio)
#   kernel/drivers/scsi/virtio_scsi.ko   (VM virtio SCSI)

# Filesystem drivers
#   kernel/fs/ext4/ext4.ko
#   kernel/fs/xfs/xfs.ko

# Device Mapper (LVM, LUKS)
#   kernel/drivers/md/dm-mod.ko          (Device Mapper core)
#   kernel/drivers/md/dm-crypt.ko        (LUKS/dm-crypt)
#   kernel/drivers/md/dm-mirror.ko
#   kernel/drivers/md/dm-snapshot.ko

# RAID (if applicable)
#   kernel/drivers/md/raid456.ko

# Keyboard (for LUKS passphrase input)
#   kernel/drivers/input/keyboard/atkbd.ko
#   kernel/drivers/hid/usbhid/usbhid.ko
```

### Herramientas de userspace

```bash
# List binaries in initramfs
lsinitrd /boot/initramfs-$(uname -r).img | grep -E "^-rwx" | head -20

# Typical binaries:
# /usr/bin/modprobe          ← Load kernel modules
# /usr/sbin/blkid            ← Identify block devices by UUID
# /usr/bin/mount              ← Mount filesystems
# /usr/bin/switch_root        ← Transition to real root
# /usr/sbin/fsck              ← Filesystem check
# /usr/sbin/cryptsetup        ← LUKS operations
# /usr/sbin/lvm               ← LVM operations (pvs, vgs, lvs)
# /usr/sbin/dmsetup           ← Device Mapper control
# /usr/lib/systemd/systemd    ← init process within initramfs (dracut)
# /usr/bin/plymouth            ← Graphical splash screen
# /usr/lib/udev/udevd         ← Device manager
```

### Archivos de configuración

```bash
# Configuration files in initramfs
lsinitrd /boot/initramfs-$(uname -r).img | grep "^-rw" | grep etc

# /etc/modprobe.d/           ← Module options and blacklists
# /etc/crypttab              ← LUKS device mapping (if encrypted)
# /etc/fstab                 ← Minimal (may include /boot)
# /etc/lvm/lvm.conf          ← LVM configuration
# /etc/vconsole.conf         ← Keyboard layout (for LUKS input)
# /etc/locale.conf           ← Locale settings
```

---

## El init del initramfs

### dracut (RHEL/Fedora)

En distribuciones basadas en dracut, el `/init` dentro del initramfs es **systemd** (una instancia temprana). El arranque dentro del initramfs usa targets y units, igual que el systemd del sistema real:

```
  ┌──────────────────────────────────────────────────────────┐
  │            SYSTEMD DENTRO DEL INITRAMFS (dracut)         │
  │                                                          │
  │  initrd.target                                           │
  │  ├── dracut-cmdline.service     (parsear cmdline)        │
  │  ├── systemd-udevd.service     (detectar hardware)       │
  │  ├── dracut-pre-udev.service   (pre-configuración)       │
  │  ├── dracut-initqueue.service  (esperar root device)     │
  │  ├── cryptsetup.target         (descifrar LUKS)          │
  │  ├── lvm2-activation.service   (activar LVM)             │
  │  ├── sysroot.mount             (montar / en /sysroot)    │
  │  └── initrd-switch-root.target (hacer switch_root)       │
  │         └── initrd-switch-root.service                   │
  │             → switch_root /sysroot /sbin/init             │
  │             → systemd del sistema real toma control       │
  └──────────────────────────────────────────────────────────┘
```

```bash
# View dracut modules included in initramfs
lsinitrd /boot/initramfs-$(uname -r).img | grep "^dracut modules"
# dracut modules:
# bash
# systemd
# systemd-initrd
# kernel-modules
# lvm
# crypt
# plymouth
# ...
```

### initramfs-tools (Debian/Ubuntu)

En Debian, el `/init` es un **script de shell** que ejecuta hooks en orden:

```
  ┌──────────────────────────────────────────────────────────┐
  │        SCRIPT /init DEL INITRAMFS (initramfs-tools)      │
  │                                                          │
  │  /init (shell script)                                    │
  │  ├── Montar /proc, /sys, /dev                            │
  │  ├── Ejecutar scripts en /scripts/init-top/              │
  │  │   (udev, plymouth, keyboard setup)                    │
  │  ├── Ejecutar scripts en /scripts/init-premount/         │
  │  │   (modules loading, RAID, LVM, LUKS)                  │
  │  ├── Montar root filesystem                              │
  │  ├── Ejecutar scripts en /scripts/init-bottom/           │
  │  │   (cleanup, prepare for switch)                       │
  │  └── exec switch_root → /sbin/init                       │
  └──────────────────────────────────────────────────────────┘
```

---

## switch_root: la transición

`switch_root` es el momento crítico donde el initramfs deja de ser `/` y el root filesystem real toma su lugar.

```
  ANTES de switch_root:
  ┌──────────────────────────────────────┐
  │  / = tmpfs (initramfs)               │
  │  ├── /init (PID 1)                   │
  │  ├── /bin, /sbin, /lib               │
  │  └── /sysroot = root real (montado)  │
  └──────────────────────────────────────┘

  switch_root ejecuta:
  1. Borrar TODOS los archivos del tmpfs (initramfs)
     → Libera la RAM que ocupaba
  2. mount --move /sysroot /
     → El root real se convierte en /
  3. exec /sbin/init
     → Reemplaza el proceso actual por systemd
     → PID 1 ahora es systemd del sistema real

  DESPUÉS de switch_root:
  ┌──────────────────────────────────────┐
  │  / = disco real (ext4/xfs)           │
  │  ├── /sbin/init → systemd (PID 1)   │
  │  ├── /bin, /sbin, /lib              │
  │  ├── /lib/modules/                   │
  │  └── (initramfs ya no existe en RAM) │
  └──────────────────────────────────────┘
```

### switch_root vs pivot_root

| | switch_root | pivot_root |
|---|---|---|
| **Usado por** | initramfs moderno | initrd antiguo |
| **Viejo root** | Borrado, RAM liberada | Queda montado en otro punto |
| **Eficiencia** | Alta (libera toda la RAM) | Baja (RAM no liberada) |
| **Limpieza** | Automática | Manual (`umount` del viejo root) |

---

## Cuándo se necesita regenerar

El initramfs se genera durante la instalación del kernel y se almacena en `/boot/`. Debe regenerarse cuando cambia algo que afecta el acceso al root filesystem durante el arranque.

### Regeneración automática

Estos eventos regeneran el initramfs automáticamente (gestionado por los scripts del paquete del kernel):

```
  ┌────────────────────────────────────────────────────────┐
  │              REGENERACIÓN AUTOMÁTICA                    │
  │                                                        │
  │  • Instalar un kernel nuevo                            │
  │    dnf install kernel / apt upgrade linux-image-*       │
  │    → Genera initramfs para el nuevo kernel             │
  │                                                        │
  │  • Actualizar un paquete que incluye dracut modules     │
  │    (plymouth, lvm2, cryptsetup, mdadm)                 │
  │    → Puede regenerar (depende de la distribución)      │
  │                                                        │
  │  • DKMS instala/actualiza un módulo                    │
  │    → Regenera si el módulo está en initramfs           │
  └────────────────────────────────────────────────────────┘
```

### Regeneración manual necesaria

Debes regenerar manualmente en estos casos:

| Situación | Por qué |
|---|---|
| **Cambiar controladora de disco** | Nuevo driver necesario (ej: IDE → virtio) |
| **Añadir LUKS a un sistema existente** | Incluir `cryptsetup` y `dm-crypt.ko` |
| **Cambiar de ext4 a XFS (o viceversa)** en root | Diferente módulo de filesystem |
| **Configurar root en LVM/RAID** | Incluir herramientas LVM/mdadm |
| **Añadir/cambiar keymap** para LUKS | Incluir mapa de teclado correcto |
| **Blacklistear un módulo** (en `/etc/modprobe.d/`) | Actualizar la copia en initramfs |
| **Cambiar opciones de módulo** (`/etc/modprobe.d/`) | Actualizar la copia en initramfs |
| **Migrar VM entre hipervisores** | Diferentes drivers (SCSI, virtio, IDE) |
| **Añadir soporte de red en boot** (NFS root, iSCSI) | Incluir drivers de red y herramientas |

### Cómo regenerar

```bash
# ── RHEL/Fedora (dracut) ──
# Regenerate for current kernel
sudo dracut --force

# Regenerate for a specific kernel
sudo dracut --force /boot/initramfs-6.7.4-200.fc39.x86_64.img 6.7.4-200.fc39.x86_64

# Regenerate for ALL installed kernels
sudo dracut --regenerate-all --force

# ── Debian/Ubuntu (initramfs-tools) ──
# Regenerate for current kernel
sudo update-initramfs -u

# Regenerate for a specific kernel
sudo update-initramfs -u -k 6.5.0-44-generic

# Regenerate for ALL installed kernels
sudo update-initramfs -u -k all
```

> **`--force`** en dracut: sin `--force`, dracut no sobrescribe un initramfs existente. Siempre úsalo al regenerar.

---

## Inspeccionar el initramfs

### Listar contenido

```bash
# ── RHEL/Fedora ──
# Full listing (like ls -la)
lsinitrd /boot/initramfs-$(uname -r).img

# List only modules
lsinitrd /boot/initramfs-$(uname -r).img | grep "\.ko"

# List dracut modules
lsinitrd /boot/initramfs-$(uname -r).img | grep "^dracut modules"

# Show a specific file from inside the initramfs
lsinitrd /boot/initramfs-$(uname -r).img -f /etc/crypttab

# ── Debian/Ubuntu ──
lsinitramfs /boot/initrd.img-$(uname -r)
lsinitramfs /boot/initrd.img-$(uname -r) | grep "\.ko"
```

### Extraer contenido

```bash
# ── RHEL/Fedora (dracut format with early cpio) ──
mkdir /tmp/initramfs-inspect && cd /tmp/initramfs-inspect

# Skip the early cpio (microcode) and extract the main cpio
/usr/lib/dracut/skipcpio /boot/initramfs-$(uname -r).img | \
    zcat | cpio -idmv 2>/dev/null

# Now explore:
ls -la
find . -name "*.ko" | head -10    # Kernel modules
cat init                           # The init script/symlink
ls -la sysroot/                    # Empty, will be mount point

# ── Debian/Ubuntu ──
mkdir /tmp/initramfs-inspect && cd /tmp/initramfs-inspect
unmkinitramfs /boot/initrd.img-$(uname -r) .

# Result: directories early/ (microcode) and main/ (rootfs)
ls main/
cat main/init | head -20
```

### Tamaño del initramfs

```bash
# Check sizes of all initramfs images
ls -lh /boot/initramfs-*.img 2>/dev/null   # RHEL
ls -lh /boot/initrd.img-* 2>/dev/null      # Debian

# Typical sizes:
# Minimal (server, no Plymouth):   20-40 MB
# Standard (with Plymouth):        40-80 MB
# With many drivers (--no-hostonly): 80-200 MB
```

### hostonly vs generic

```
  ┌────────────────────────────────────────────────────────────┐
  │                                                            │
  │  HOSTONLY (por defecto en Fedora/RHEL)                     │
  │  • Solo incluye módulos para el hardware detectado         │
  │  • Más pequeño (20-50 MB)                                  │
  │  • Arranca más rápido (menos módulos que cargar)            │
  │  • NO portátil: falla si cambias hardware o mueves disco   │
  │                                                            │
  │  GENERIC (por defecto en Debian)                           │
  │  • Incluye módulos para una amplia variedad de hardware    │
  │  • Más grande (60-200 MB)                                  │
  │  • Arranca un poco más lento                               │
  │  • Portátil: funciona en hardware diferente                │
  │                                                            │
  └────────────────────────────────────────────────────────────┘
```

```bash
# RHEL/Fedora: generate a generic (non-hostonly) initramfs
sudo dracut --no-hostonly --force /boot/initramfs-$(uname -r).img

# Debian: initramfs-tools generates generic by default
# To change: edit /etc/initramfs-tools/initramfs.conf
# MODULES=most    (generic, default)
# MODULES=dep     (hostonly equivalent)
```

---

## Arranque sin initramfs

Técnicamente es posible arrancar Linux sin initramfs si **todos** los drivers necesarios para acceder al root filesystem están compilados directamente en el kernel (`=y`, no `=m`):

```
  ┌────────────────────────────────────────────────────────┐
  │          ARRANQUE SIN INITRAMFS                         │
  │                                                        │
  │  Requisitos:                                           │
  │  • Driver de disco built-in (ej: CONFIG_ATA_PIIX=y)   │
  │  • Driver de filesystem built-in (ej: CONFIG_EXT4=y)  │
  │  • Root en partición simple (no LVM, no RAID, no LUKS)│
  │  • No necesitar NFS root ni iSCSI                     │
  │  • No necesitar Plymouth (splash screen)               │
  │                                                        │
  │  En GRUB: eliminar la línea "initrd"                   │
  │  El kernel monta root= directamente                    │
  │                                                        │
  │  Casos prácticos:                                      │
  │  • Sistemas embebidos con kernel personalizado         │
  │  • Contenedores (sin arranque real)                    │
  │  • VMs minimalistas con kernel compilado ad-hoc        │
  └────────────────────────────────────────────────────────┘
```

En la práctica, todas las distribuciones usan initramfs porque compilan la mayoría de drivers como módulos. Intentar evitarlo con un kernel de distribución es impracticable.

---

## Errores comunes

### 1. No regenerar initramfs al cambiar hardware de almacenamiento

Si migras una VM de IDE a virtio (o cambias la controladora SATA), el initramfs no tiene el driver nuevo. El kernel arranca pero no puede encontrar el disco:

```
dracut-initqueue: Warning: Could not boot.
dracut-initqueue: Warning: /dev/disk/by-uuid/... does not exist

# Fix from rescue: regenerate with the new driver
sudo dracut --force --add-drivers "virtio_blk virtio_scsi" \
    /boot/initramfs-$(uname -r).img
```

### 2. Confundir "el initramfs está corrupto" con otros problemas

Si el initramfs está corrupto, verás errores del kernel al intentar descomprimirlo:

```
Initramfs unpacking failed: invalid magic at start of compressed archive
# or:
Initramfs unpacking failed: junk in compressed archive
```

Solución: regenerar desde un entorno de rescate (live USB o kernel anterior).

### 3. No incluir el keymap correcto para LUKS

Si tu teclado no es US-English y necesitas introducir una passphrase de LUKS durante el arranque, el initramfs debe incluir tu mapa de teclado:

```bash
# Verify keyboard layout in initramfs
lsinitrd /boot/initramfs-$(uname -r).img -f /etc/vconsole.conf
# KEYMAP=es    ← Spanish layout

# If missing or wrong:
# 1. Set the correct keymap
sudo localectl set-keymap es

# 2. Regenerate initramfs
sudo dracut --force
```

### 4. Asumir que los cambios en /etc/modprobe.d/ aplican al boot

Los cambios en `/etc/modprobe.d/` (blacklists, opciones) afectan módulos cargados **después** del arranque. Para que apliquen **durante** el arranque (dentro del initramfs), debes regenerar:

```bash
# After adding /etc/modprobe.d/blacklist-nouveau.conf:
sudo dracut --force          # RHEL
sudo update-initramfs -u     # Debian
```

### 5. Llenar /boot y no poder generar el initramfs nuevo

`/boot` suele ser una partición separada pequeña (500 MB-1 GB). Con varios kernels, puede llenarse. La generación de un nuevo initramfs falla por falta de espacio:

```bash
# Check /boot usage
df -h /boot

# Remove old kernels to free space
# RHEL:
sudo dnf remove --oldinstallonly
# Debian:
sudo apt autoremove --purge
```

---

## Cheatsheet

```bash
# ═══════════════════════════════════════════════
#  INSPECCIONAR
# ═══════════════════════════════════════════════
lsinitrd /boot/initramfs-$(uname -r).img           # RHEL: list all
lsinitramfs /boot/initrd.img-$(uname -r)            # Debian: list all
lsinitrd ... | grep "\.ko"                          # List modules
lsinitrd ... | grep "^dracut modules"               # Dracut modules
lsinitrd ... -f /etc/crypttab                       # Show specific file
ls -lh /boot/initramfs-*.img                        # Image sizes

# ═══════════════════════════════════════════════
#  REGENERAR
# ═══════════════════════════════════════════════
# RHEL/Fedora:
sudo dracut --force                                 # Current kernel
sudo dracut --force <path> <kernel-version>         # Specific kernel
sudo dracut --regenerate-all --force                # All kernels
sudo dracut --no-hostonly --force                    # Generic (portable)
sudo dracut --force --add-drivers "mod1 mod2"       # Add extra drivers

# Debian/Ubuntu:
sudo update-initramfs -u                            # Current kernel
sudo update-initramfs -u -k <version>               # Specific kernel
sudo update-initramfs -u -k all                     # All kernels

# ═══════════════════════════════════════════════
#  EXTRAER PARA INSPECCIÓN
# ═══════════════════════════════════════════════
# RHEL:
/usr/lib/dracut/skipcpio /boot/initramfs-$(uname -r).img | \
    zcat | cpio -idmv -D /tmp/initramfs-inspect
# Debian:
unmkinitramfs /boot/initrd.img-$(uname -r) /tmp/initramfs-inspect

# ═══════════════════════════════════════════════
#  DIAGNÓSTICO
# ═══════════════════════════════════════════════
file /boot/initramfs-$(uname -r).img               # Verify format
df -h /boot                                         # Check /boot space
journalctl -b | grep -i initr                       # Boot initramfs logs
dmesg | grep -i "initramfs\|initrd\|unpack"         # Kernel extraction msgs
```

---

## Ejercicios

### Ejercicio 1: Inspeccionar tu initramfs

```bash
# Step 1: find your initramfs
ls -lh /boot/initramfs-$(uname -r).img 2>/dev/null || \
    ls -lh /boot/initrd.img-$(uname -r)

# Step 2: check the file format
file /boot/initramfs-$(uname -r).img 2>/dev/null || \
    file /boot/initrd.img-$(uname -r)

# Step 3: list dracut modules (RHEL/Fedora)
lsinitrd /boot/initramfs-$(uname -r).img 2>/dev/null | head -30

# Step 4: find storage modules
lsinitrd /boot/initramfs-$(uname -r).img 2>/dev/null | \
    grep -E "nvme|ahci|sd_mod|virtio|xfs|ext4" || \
lsinitramfs /boot/initrd.img-$(uname -r) 2>/dev/null | \
    grep -E "nvme|ahci|sd_mod|virtio|xfs|ext4"

# Step 5: check for LUKS/LVM tools
lsinitrd /boot/initramfs-$(uname -r).img 2>/dev/null | \
    grep -E "cryptsetup|lvm|dmsetup"

# Step 6: view the init process type
lsinitrd /boot/initramfs-$(uname -r).img -f /init 2>/dev/null | head -5

# Step 7: compare sizes of different kernel initramfs
ls -lh /boot/initramfs-*.img 2>/dev/null || \
    ls -lh /boot/initrd.img-*

# Step 8: count total modules
lsinitrd /boot/initramfs-$(uname -r).img 2>/dev/null | \
    grep -c "\.ko" || \
lsinitramfs /boot/initrd.img-$(uname -r) 2>/dev/null | \
    grep -c "\.ko"
```

**Preguntas:**
1. ¿Cuántos módulos `.ko` contiene tu initramfs? ¿Cuántos módulos tiene `/lib/modules/$(uname -r)/` en total?
2. ¿Tu initramfs incluye drivers de red? ¿Por qué sí o por qué no?
3. ¿Incluye `cryptsetup`? ¿Tu sistema usa LUKS?

> **Pregunta de predicción**: si generas un initramfs con `--no-hostonly` (dracut), ¿esperas que sea más grande o más pequeño? ¿Aproximadamente cuánto? Verifica generándolo en un directorio temporal: `sudo dracut --no-hostonly /tmp/test-initramfs.img $(uname -r)` y comparando tamaños.

### Ejercicio 2: Extraer y explorar el initramfs

```bash
# Step 1: create a working directory
mkdir -p /tmp/initramfs-lab && cd /tmp/initramfs-lab

# Step 2: extract the initramfs
# RHEL/Fedora:
/usr/lib/dracut/skipcpio /boot/initramfs-$(uname -r).img | \
    zcat | cpio -idmv 2>/dev/null
# Debian/Ubuntu:
# unmkinitramfs /boot/initrd.img-$(uname -r) .

# Step 3: explore the structure
find . -maxdepth 2 -type d | sort

# Step 4: find the init process
ls -la init
file init
# Is it a script or a symlink to systemd?

# Step 5: list all executable binaries
find . -type f -executable | sort

# Step 6: find the storage modules
find . -name "*.ko" -path "*/drivers/*" | sort

# Step 7: check if microcode is included
find . -path "*/microcode/*" -type f

# Step 8: check the etc/ configuration
find . -path "*/etc/*" -type f | sort
cat etc/vconsole.conf 2>/dev/null

# Step 9: clean up
cd / && rm -rf /tmp/initramfs-lab
```

**Preguntas:**
1. ¿`/init` es un script shell o un symlink a systemd? ¿Qué distribución usas?
2. ¿Encontraste archivos de microcode de CPU? ¿Intel o AMD?
3. ¿Cuántos binarios ejecutables hay? ¿Reconoces para qué sirve cada uno?

> **Pregunta de predicción**: si dentro del initramfs extraído eliminas el módulo `ext4.ko` (o `xfs.ko`, según tu filesystem) y empaquetas el initramfs de vuelta, ¿qué error esperas al arrancar? ¿En qué fase se detendrá el sistema?

### Ejercicio 3: Regenerar y comparar initramfs

```bash
# Step 1: record the current initramfs size
CURRENT=$(ls -l /boot/initramfs-$(uname -r).img | awk '{print $5}')
echo "Current size: $CURRENT bytes ($(($CURRENT / 1024 / 1024)) MB)"

# Step 2: backup the current initramfs
sudo cp /boot/initramfs-$(uname -r).img /boot/initramfs-$(uname -r).img.bak

# Step 3: regenerate (hostonly, default)
sudo dracut --force /boot/initramfs-$(uname -r).img
NEW_HOSTONLY=$(ls -l /boot/initramfs-$(uname -r).img | awk '{print $5}')
echo "Hostonly size: $NEW_HOSTONLY bytes ($(($NEW_HOSTONLY / 1024 / 1024)) MB)"

# Step 4: generate a generic (non-hostonly) version to /tmp
sudo dracut --no-hostonly /tmp/initramfs-generic.img $(uname -r)
GENERIC=$(ls -l /tmp/initramfs-generic.img | awk '{print $5}')
echo "Generic size: $GENERIC bytes ($(($GENERIC / 1024 / 1024)) MB)"

# Step 5: compare module counts
echo "Hostonly modules:"
lsinitrd /boot/initramfs-$(uname -r).img | grep -c "\.ko"
echo "Generic modules:"
lsinitrd /tmp/initramfs-generic.img | grep -c "\.ko"

# Step 6: restore backup
sudo cp /boot/initramfs-$(uname -r).img.bak /boot/initramfs-$(uname -r).img

# Step 7: clean up
rm -f /tmp/initramfs-generic.img
sudo rm -f /boot/initramfs-$(uname -r).img.bak
```

**Preguntas:**
1. ¿Cuántas veces más grande es el initramfs genérico respecto al hostonly?
2. ¿Cuántos módulos adicionales incluye el genérico?
3. ¿En qué escenario necesitarías el genérico?

> **Pregunta de predicción**: si regeneras el initramfs con `sudo dracut --add-drivers "bnxt_en e1000e igb" --force`, ¿qué tipo de drivers estarías añadiendo? ¿Por qué querrías incluir drivers de red en el initramfs si tu root no es NFS?

---

> **Siguiente tema**: T02 — dracut (RHEL) vs update-initramfs (Debian): regeneración, módulos, troubleshooting.
