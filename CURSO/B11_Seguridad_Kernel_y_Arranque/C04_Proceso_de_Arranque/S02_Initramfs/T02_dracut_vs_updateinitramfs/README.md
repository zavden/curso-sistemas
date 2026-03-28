# dracut (RHEL) vs update-initramfs (Debian)

## Índice
1. [Dos ecosistemas, un mismo problema](#dos-ecosistemas-un-mismo-problema)
2. [dracut: arquitectura modular](#dracut-arquitectura-modular)
3. [dracut: uso y opciones](#dracut-uso-y-opciones)
4. [dracut: configuración persistente](#dracut-configuración-persistente)
5. [initramfs-tools: arquitectura por hooks](#initramfs-tools-arquitectura-por-hooks)
6. [update-initramfs: uso y opciones](#update-initramfs-uso-y-opciones)
7. [initramfs-tools: configuración](#initramfs-tools-configuración)
8. [Comparación directa](#comparación-directa)
9. [Troubleshooting del initramfs](#troubleshooting-del-initramfs)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## Dos ecosistemas, un mismo problema

Ambas herramientas resuelven lo mismo — generar un initramfs que permita al kernel acceder al root filesystem — pero con filosofías diferentes:

```
  ┌──────────────────────────────────────────────────────────────┐
  │                                                              │
  │  RHEL / Fedora / SUSE                Debian / Ubuntu         │
  │  ┌──────────────────────┐            ┌────────────────────┐  │
  │  │       dracut          │            │  initramfs-tools   │  │
  │  │                      │            │                    │  │
  │  │ • Modular (plugins)  │            │ • Hooks (scripts)  │  │
  │  │ • systemd inside     │            │ • Shell script /init│  │
  │  │ • hostonly default   │            │ • Generic default  │  │
  │  │ • Event-driven       │            │ • Sequential       │  │
  │  │ • dracut modules     │            │ • Hook directories │  │
  │  └──────────────────────┘            └────────────────────┘  │
  │         │                                     │              │
  │         ▼                                     ▼              │
  │   dracut [options]                   update-initramfs [opts] │
  │   lsinitrd                           lsinitramfs             │
  │                                      unmkinitramfs           │
  │                                                              │
  │  Ambos producen:                                             │
  │  /boot/initramfs-<version>.img   /boot/initrd.img-<version> │
  │  (archivo cpio comprimido)       (archivo cpio comprimido)   │
  └──────────────────────────────────────────────────────────────┘
```

---

## dracut: arquitectura modular

dracut organiza la funcionalidad en **módulos** (no confundir con módulos del kernel). Cada módulo dracut es un directorio con scripts que saben cómo configurar un aspecto específico del arranque.

### Estructura de módulos

```bash
ls /usr/lib/dracut/modules.d/
# 00bash/              ← Shell para debugging
# 00systemd/           ← systemd como init del initramfs
# 01systemd-initrd/    ← Targets de systemd para initrd
# 03modsign/           ← Verificación de firma de módulos
# 04watchdog/          ← Hardware watchdog
# 10i18n/              ← Internacionalización (keymaps, fonts)
# 50drm/               ← GPU drivers (para Plymouth)
# 90crypt/             ← LUKS/dm-crypt
# 90dm/                ← Device Mapper
# 90lvm/               ← LVM
# 90mdraid/            ← Software RAID
# 91crypt-gpg/         ← LUKS con GPG keyfile
# 91crypt-loop/        ← LUKS con loop device
# 95nfs/               ← NFS root
# 95iscsi/             ← iSCSI root
# 95rootfs-block/      ← Root en block device (lo común)
# 98dracut-systemd/    ← Integración dracut-systemd
# 98plymouth/          ← Splash screen
# 99base/              ← Funcionalidad base (siempre incluido)
```

Cada módulo tiene una estructura interna:

```
  /usr/lib/dracut/modules.d/90crypt/
  ├── module-setup.sh      ← Define dependencias y qué instalar
  │                          check(): ¿debe incluirse este módulo?
  │                          depends(): ¿qué otros módulos necesita?
  │                          install(): copiar binarios/configs
  │                          installkernel(): copiar módulos .ko
  ├── crypt-run-generator.sh
  ├── crypt-cleanup.sh
  ├── cryptroot-ask.sh     ← Pide passphrase de LUKS
  └── parse-crypt.sh       ← Parsea rd.luks.* del cmdline
```

### Cómo decide dracut qué incluir

```
  ┌─────────────────────────────────────────────────────────┐
  │            DECISIÓN DE INCLUSIÓN                         │
  │                                                         │
  │  Para cada módulo en modules.d/:                        │
  │                                                         │
  │  1. Ejecutar check()                                    │
  │     → ¿Las herramientas necesarias están instaladas?    │
  │     → ¿El hardware relevante existe? (hostonly mode)    │
  │     → return 0 (incluir) o return 255 (omitir)         │
  │                                                         │
  │  2. Si check() dice sí, resolver depends()             │
  │     → "90crypt" depende de "dm" y "kernel-modules"     │
  │     → Incluir dependencias recursivamente              │
  │                                                         │
  │  3. Ejecutar installkernel()                            │
  │     → Copiar módulos .ko necesarios                    │
  │                                                         │
  │  4. Ejecutar install()                                  │
  │     → Copiar binarios, scripts, configs                │
  │                                                         │
  │  Forzar inclusión/exclusión:                            │
  │  --add "module"       → incluir aunque check() diga no │
  │  --omit "module"      → excluir aunque check() diga sí │
  │  --force-add "module" → incluir sin verificar check()  │
  └─────────────────────────────────────────────────────────┘
```

---

## dracut: uso y opciones

### Comandos básicos

```bash
# Regenerate initramfs for current kernel (overwrite existing)
sudo dracut --force

# Regenerate for a specific kernel version
sudo dracut --force /boot/initramfs-6.7.4-200.fc39.x86_64.img \
    6.7.4-200.fc39.x86_64

# Regenerate ALL installed kernels
sudo dracut --regenerate-all --force

# Generate to a temporary location (testing)
sudo dracut --force /tmp/test-initramfs.img $(uname -r)
```

### Opciones de contenido

```bash
# ── Módulos dracut ──

# Add a specific dracut module
sudo dracut --force --add "crypt lvm"

# Remove (omit) a dracut module
sudo dracut --force --omit "plymouth"

# List modules that would be included (dry run)
dracut --list-modules

# Force-add even if check() fails
sudo dracut --force --force-add "nfs"

# ── Módulos del kernel ──

# Add extra kernel modules
sudo dracut --force --add-drivers "bnxt_en e1000e igb"

# Force include specific kernel modules (ignore soft dependencies)
sudo dracut --force --force-drivers "nvme"

# Omit kernel modules
sudo dracut --force --omit-drivers "nouveau"

# ── Modo hostonly vs genérico ──

# Generate hostonly (default on Fedora/RHEL)
sudo dracut --force --hostonly

# Generate generic (include drivers for many systems)
sudo dracut --force --no-hostonly

# Hostonly with extra drivers as fallback
sudo dracut --force --hostonly --hostonly-mode "strict"
```

### Opciones de diagnóstico

```bash
# Verbose output during generation
sudo dracut --force --verbose

# Even more verbose
sudo dracut --force -v -v

# Show which modules are included and why
sudo dracut --force --print-cmdline

# Log to a file
sudo dracut --force --logfile /tmp/dracut.log

# Generate with debugging shell on failure (rd.shell in cmdline)
# This is a kernel param, not a dracut option:
# In GRUB: add rd.shell rd.debug to the linux line
```

### Opciones de compresión

```bash
# Choose compression algorithm
sudo dracut --force --compress=gzip     # Default, good compatibility
sudo dracut --force --compress=xz       # Smaller, slower to decompress
sudo dracut --force --compress=lz4      # Fastest decompression
sudo dracut --force --compress=zstd     # Good balance (Fedora default)
sudo dracut --force --no-compress       # No compression (fastest boot, largest)

# Typical size comparison for the same content:
# no-compress:  150 MB
# gzip:          50 MB
# zstd:          45 MB
# xz:            35 MB
# lz4:           60 MB  (but decompresses 3x faster than gzip)
```

---

## dracut: configuración persistente

En lugar de pasar opciones cada vez, puedes configurar dracut permanentemente.

### /etc/dracut.conf

```bash
# /etc/dracut.conf — Global configuration
# (applies to ALL dracut invocations)

# Compression
compress="zstd"

# Hostonly mode
hostonly="yes"
hostonly_cmdline="yes"

# Additional dracut modules to always include
add_dracutmodules+=" crypt lvm "

# Dracut modules to never include
omit_dracutmodules+=" plymouth "

# Additional kernel modules
add_drivers+=" bnxt_en e1000e "

# Kernel modules to exclude
omit_drivers+=" nouveau "

# Include firmware files
install_items+=" /etc/crypttab "

# Force inclusion of files
install_items+=" /etc/vconsole.conf /etc/locale.conf "
```

### /etc/dracut.conf.d/*.conf

Archivos modulares que se procesan en orden alfabético (preferido sobre `/etc/dracut.conf`):

```bash
ls /etc/dracut.conf.d/
# 01-dist.conf          ← Defaults de la distribución
# 50-nfs.conf           ← Configuración personalizada para NFS
# 99-custom.conf        ← Mis customizaciones

# Example: /etc/dracut.conf.d/99-custom.conf
cat > /etc/dracut.conf.d/99-custom.conf << 'EOF'
# Always include LUKS support even if no encrypted disks detected
add_dracutmodules+=" crypt "
# Use zstd compression for faster boot
compress="zstd"
# Omit Plymouth for servers
omit_dracutmodules+=" plymouth "
EOF
```

### Kernel command line parameters de dracut

dracut responde a parámetros en la línea de comandos del kernel (pasados desde GRUB):

```bash
# ── Debugging ──
rd.shell                    # Drop to shell on failure
rd.debug                    # Verbose debug logging
rd.break                    # Break before switch_root
rd.break=pre-mount          # Break before mounting root
rd.break=pre-pivot          # Break before pivot_root
rd.break=pre-udev           # Break before udev starts

# ── Root device ──
root=UUID=...               # Root by UUID
root=LABEL=...              # Root by label
root=/dev/sda3              # Root by device path
root=/dev/mapper/vg-root    # Root on LVM
rootfstype=ext4             # Force filesystem type
rootflags=ro,noatime        # Mount options for root

# ── LUKS ──
rd.luks.uuid=<uuid>         # LUKS device to unlock
rd.luks.key=<keyfile>       # Keyfile path
rd.luks.options=<opts>      # cryptsetup options

# ── LVM ──
rd.lvm.vg=<vgname>          # Activate specific VG
rd.lvm.lv=<vg/lv>           # Activate specific LV

# ── Network ──
rd.neednet=1                # Force network initialization
ip=dhcp                     # Configure network via DHCP
ip=<ip>::<gw>:<mask>::<iface>:none  # Static IP

# ── Timeouts ──
rd.timeout=30               # Seconds to wait for root device
rd.retry=5                  # Retry count for root device
```

---

## initramfs-tools: arquitectura por hooks

initramfs-tools usa una arquitectura más simple basada en **hooks** — scripts de shell ejecutados en puntos específicos del proceso de arranque.

### Estructura

```
  /etc/initramfs-tools/
  ├── initramfs.conf         ← Configuración principal
  ├── modules                ← Módulos del kernel a incluir
  ├── conf.d/                ← Configuración modular
  ├── hooks/                 ← Scripts durante GENERACIÓN
  │   ├── fsck               ← Instalar herramientas de fsck
  │   ├── resume             ← Soporte para hibernación
  │   └── ...
  └── scripts/               ← Scripts durante ARRANQUE
      ├── init-top/          ← Antes de todo (udev, keymap)
      ├── init-premount/     ← Antes de montar root (LVM, LUKS, RAID)
      ├── local-top/         ← Antes de montar root local
      ├── local-premount/    ← Justo antes de mount root
      ├── local-bottom/      ← Después de montar root local
      ├── init-bottom/       ← Después de montar root (cleanup)
      ├── nfs-top/           ← Para NFS root
      ├── nfs-premount/
      └── nfs-bottom/
```

### hooks vs scripts

```
  ┌────────────────────────────────────────────────────────────┐
  │                                                            │
  │  HOOKS (/etc/initramfs-tools/hooks/)                       │
  │  Se ejecutan durante la GENERACIÓN del initramfs           │
  │  (en el host, con acceso completo al sistema)              │
  │                                                            │
  │  Propósito:                                                │
  │  • Copiar binarios al initramfs                            │
  │  • Copiar módulos del kernel                               │
  │  • Copiar archivos de configuración                        │
  │  • Preparar lo que el script de arranque necesitará        │
  │                                                            │
  │  Usan funciones helper:                                    │
  │  . /usr/share/initramfs-tools/hook-functions               │
  │  copy_exec /usr/sbin/cryptsetup /sbin                      │
  │  manual_add_modules dm_crypt                               │
  │  add_modules_from_file /etc/initramfs-tools/modules        │
  │                                                            │
  ├────────────────────────────────────────────────────────────┤
  │                                                            │
  │  SCRIPTS (/etc/initramfs-tools/scripts/*)                  │
  │  Se ejecutan durante el ARRANQUE                           │
  │  (dentro del initramfs, entorno limitado)                  │
  │                                                            │
  │  Propósito:                                                │
  │  • Cargar módulos                                          │
  │  • Ensamblar RAID/LVM                                      │
  │  • Descifrar LUKS                                          │
  │  • Montar el root filesystem                               │
  │  • Preparar para switch_root                               │
  │                                                            │
  └────────────────────────────────────────────────────────────┘
```

### Flujo de arranque con initramfs-tools

```
  /init (shell script)
       │
       ├── mount /proc, /sys, /dev
       │
       ├── run scripts in /scripts/init-top/
       │   ├── udev (start device detection)
       │   ├── keymap (load keyboard layout)
       │   └── plymouth (start splash)
       │
       ├── Wait for root device to appear
       │   (poll /dev/ using udev events)
       │
       ├── run scripts in /scripts/local-top/
       │   ├── cryptroot (LUKS decrypt)
       │   ├── lvm2 (activate VGs)
       │   └── mdadm (assemble RAID)
       │
       ├── run scripts in /scripts/local-premount/
       │   └── resume (check for hibernation image)
       │
       ├── mount root filesystem
       │   mount -o ${ROOTFLAGS} ${ROOT} ${rootmnt}
       │
       ├── run scripts in /scripts/local-bottom/
       │
       ├── run scripts in /scripts/init-bottom/
       │   └── udev (stop, move to real root)
       │
       └── exec switch_root ${rootmnt} /sbin/init
```

---

## update-initramfs: uso y opciones

### Comandos básicos

```bash
# Update (regenerate) initramfs for current kernel
sudo update-initramfs -u

# Update for a specific kernel version
sudo update-initramfs -u -k 6.5.0-44-generic

# Update ALL installed kernels
sudo update-initramfs -u -k all

# Create a new initramfs (used during kernel install)
sudo update-initramfs -c -k 6.5.0-44-generic

# Delete an initramfs
sudo update-initramfs -d -k 6.5.0-44-generic

# Verbose output
sudo update-initramfs -u -v
```

### mkinitramfs: el generador subyacente

`update-initramfs` es un wrapper. El generador real es `mkinitramfs`:

```bash
# Direct generation (rarely needed, but useful for debugging)
sudo mkinitramfs -o /tmp/test-initrd.img $(uname -r)

# With verbose output
sudo mkinitramfs -v -o /tmp/test-initrd.img $(uname -r)
```

---

## initramfs-tools: configuración

### /etc/initramfs-tools/initramfs.conf

```bash
# /etc/initramfs-tools/initramfs.conf

# Module inclusion strategy
MODULES=most
# most    = generic: include drivers for most hardware (DEFAULT)
# dep     = hostonly: only drivers for detected hardware
# list    = only modules listed in /etc/initramfs-tools/modules
# netboot = drivers for network boot

# Root filesystem type
# (usually auto-detected, override if needed)
# FSTYPE=ext4

# Compression
COMPRESS=zstd
# gzip (default), lz4, lzma, lzo, xz, zstd

# Resume device (for hibernation)
RESUME=/dev/mapper/vg-swap
# Or: RESUME=UUID=...
# Or: RESUME=none (disable)

# NFS boot options
# BOOT=local (default), nfs
BOOT=local

# Busybox (always included by default)
BUSYBOX=auto
```

### /etc/initramfs-tools/modules

Lista explícita de módulos del kernel para incluir siempre:

```bash
# /etc/initramfs-tools/modules
# Modules listed here will be included in the initramfs
# One module per line, options can follow the module name

# Force include a storage driver
nvme
ahci

# Include with options
dm_crypt

# Include network driver (for remote LUKS unlock, etc)
e1000e
```

### /etc/initramfs-tools/conf.d/

```bash
# Modular configuration (overrides initramfs.conf)
ls /etc/initramfs-tools/conf.d/
# resume              ← RESUME=UUID=... (auto-generated)
# driver-policy       ← Custom MODULES= setting

cat /etc/initramfs-tools/conf.d/resume
# RESUME=UUID=a1b2c3d4-...
```

---

## Comparación directa

### Tabla de equivalencias

| Tarea | dracut (RHEL/Fedora) | initramfs-tools (Debian/Ubuntu) |
|---|---|---|
| **Regenerar actual** | `sudo dracut --force` | `sudo update-initramfs -u` |
| **Regenerar específico** | `sudo dracut --force <path> <ver>` | `sudo update-initramfs -u -k <ver>` |
| **Regenerar todos** | `sudo dracut --regenerate-all --force` | `sudo update-initramfs -u -k all` |
| **Listar contenido** | `lsinitrd <img>` | `lsinitramfs <img>` |
| **Extraer** | `skipcpio <img> \| zcat \| cpio -id` | `unmkinitramfs <img> <dir>` |
| **Ver archivo interno** | `lsinitrd <img> -f /path` | Extraer + cat |
| **Modo hostonly** | `--hostonly` (default) | `MODULES=dep` en initramfs.conf |
| **Modo genérico** | `--no-hostonly` | `MODULES=most` (default) |
| **Añadir módulo kernel** | `--add-drivers "mod"` | Añadir a `/etc/initramfs-tools/modules` |
| **Omitir módulo kernel** | `--omit-drivers "mod"` | Blacklist en `/etc/modprobe.d/` |
| **Añadir funcionalidad** | `--add "crypt"` (dracut module) | Instalar paquete (ej: `cryptsetup-initramfs`) |
| **Config persistente** | `/etc/dracut.conf.d/*.conf` | `/etc/initramfs-tools/initramfs.conf` |
| **Compresión** | `--compress=zstd` | `COMPRESS=zstd` en initramfs.conf |
| **Verbose** | `--verbose` o `-v` | `-v` |
| **Init process** | systemd (dentro del initramfs) | Script shell `/init` |
| **Imagen result.** | `/boot/initramfs-<ver>.img` | `/boot/initrd.img-<ver>` |

### Diferencias arquitectónicas

```
  ┌────────────────────────────────────────────────────────────┐
  │                                                            │
  │  dracut                          initramfs-tools           │
  │                                                            │
  │  ┌──────────────────┐            ┌──────────────────┐      │
  │  │ module-setup.sh  │            │    hooks/        │      │
  │  │  check()         │            │  (generación)    │      │
  │  │  depends()       │            │                  │      │
  │  │  install()       │            │    scripts/      │      │
  │  │  installkernel() │            │  (arranque)      │      │
  │  └──────────────────┘            └──────────────────┘      │
  │  Cada módulo se auto-            Hooks y scripts son       │
  │  describe: sabe si debe          independientes.           │
  │  incluirse y qué necesita.       La lógica está en el      │
  │                                  script /init.             │
  │  Init = systemd                  Init = shell script       │
  │  (paralelización,                (secuencial, simple,      │
  │   journal, targets)              fácil de leer/debug)      │
  │                                                            │
  │  Pro: más potente,               Pro: más simple,          │
  │  mejor para configs              más fácil de              │
  │  complejas (LUKS+LVM+RAID)       personalizar              │
  │                                                            │
  │  Con: más complejo,              Con: menos features,      │
  │  debugging más difícil           no paraleliza             │
  │  (systemd journal dentro         (arranque un poco         │
  │   de initramfs)                  más lento)                │
  └────────────────────────────────────────────────────────────┘
```

### Integración con el sistema de paquetes

```
  ┌───────────────────────────────────────────────────────────┐
  │  RHEL/Fedora                                              │
  │                                                           │
  │  dnf install kernel-6.8.0                                 │
  │       │                                                   │
  │       ├── Instala /boot/vmlinuz-6.8.0                     │
  │       ├── Instala /lib/modules/6.8.0/                     │
  │       ├── Ejecuta: dracut /boot/initramfs-6.8.0.img 6.8.0│
  │       ├── Ejecuta: grubby (actualiza BLS entry)           │
  │       └── Resultado: listo para reboot                    │
  │                                                           │
  ├───────────────────────────────────────────────────────────┤
  │  Debian/Ubuntu                                            │
  │                                                           │
  │  apt install linux-image-6.5.0-44                         │
  │       │                                                   │
  │       ├── Instala /boot/vmlinuz-6.5.0-44                  │
  │       ├── Instala /lib/modules/6.5.0-44/                  │
  │       ├── Ejecuta: update-initramfs -c -k 6.5.0-44       │
  │       ├── Ejecuta: update-grub                            │
  │       └── Resultado: listo para reboot                    │
  └───────────────────────────────────────────────────────────┘
```

---

## Troubleshooting del initramfs

### El sistema no arranca: ¿es el initramfs?

```
  ┌────────────────────────────────────────────────────────────┐
  │  SÍNTOMA                         │  ¿ES EL INITRAMFS?      │
  │                                  │                         │
  │  "Initramfs unpacking failed"    │  SÍ — imagen corrupta   │
  │                                  │                         │
  │  "dracut-initqueue timeout"      │  SÍ — no encuentra root │
  │  "Gave up waiting for root       │  SÍ — no encuentra root │
  │   file system device"            │                         │
  │                                  │                         │
  │  Drops to dracut shell           │  SÍ — error durante     │
  │  (dracut:/#)                     │  el proceso del initramfs│
  │                                  │                         │
  │  Drops to (initramfs) shell      │  SÍ — (Debian equiv.)   │
  │                                  │                         │
  │  "VFS: Unable to mount root fs"  │  QUIZÁS — kernel no     │
  │                                  │  tiene soporte, o no hay│
  │                                  │  initramfs              │
  │                                  │                         │
  │  "Kernel panic - not syncing"    │  QUIZÁS — depende del   │
  │                                  │  mensaje completo       │
  └────────────────────────────────────────────────────────────┘
```

### dracut shell de emergencia

Cuando dracut falla, ofrece una shell para diagnóstico (si `rd.shell` está habilitado, que es el default):

```bash
# You see:
# dracut-initqueue[xxx]: Warning: dracut-initqueue timeout
# ...
# dracut:/#

# Available commands in dracut shell:
journalctl             # View full boot log (systemd inside initramfs)
cat /run/initramfs/rdsosreport.txt   # Detailed diagnostics
lsblk                  # List block devices
blkid                  # Show UUIDs
ls /dev/mapper/        # LVM/LUKS devices
cat /proc/cmdline      # Kernel parameters
dmesg | tail -30       # Recent kernel messages

# Common issue: wrong root= parameter
cat /proc/cmdline | grep root=
blkid | grep -i "uuid"
# Compare: does root=UUID=xxx match any actual UUID?

# Common issue: missing driver
lsmod                  # What's loaded?
ls /lib/modules/*/kernel/drivers/   # What's available?

# Try to fix and continue:
# If root device exists but wasn't found in time:
mount /dev/sda3 /sysroot
exit   # Continue boot

# If unfixable: reboot with different params
reboot -f
```

### Shell de initramfs-tools (Debian)

```bash
# You see:
# ALERT! /dev/disk/by-uuid/xxx does not exist.
# Dropping to a shell!
# (initramfs)

# Available commands:
cat /proc/cmdline
blkid
ls /dev/
lsmod
modprobe <driver>      # Try loading a missing driver

# Manual mount and continue:
mount /dev/sda3 /root
exit   # Continue boot
```

### Debugging detallado con rd.debug (dracut)

```bash
# In GRUB, add to linux line:
rd.debug rd.shell

# This produces extensive logging:
# - Every dracut module action is logged
# - Every command execution is traced
# - Output goes to /run/initramfs/init.log
# - And to the console (very verbose)

# After boot (successful or not), examine:
cat /run/initramfs/rdsosreport.txt    # Comprehensive report
journalctl -b | grep dracut           # dracut-specific messages
```

### Regenerar desde un entorno de rescate

```bash
# Boot from live USB or rescue kernel, then:

# 1. Mount the real root
sudo mount /dev/sda3 /mnt
sudo mount /dev/sda2 /mnt/boot       # If /boot is separate
sudo mount /dev/sda1 /mnt/boot/efi   # If UEFI

# 2. Mount pseudo-filesystems
sudo mount --bind /dev  /mnt/dev
sudo mount --bind /proc /mnt/proc
sudo mount --bind /sys  /mnt/sys
sudo mount --bind /run  /mnt/run

# 3. Chroot
sudo chroot /mnt

# 4. Regenerate
# RHEL:
dracut --force /boot/initramfs-$(uname -r).img $(uname -r)
# Note: $(uname -r) is the RESCUE kernel, you may need to
# specify the target kernel version explicitly:
ls /lib/modules/    # Find installed kernel versions
dracut --force /boot/initramfs-6.7.4-200.fc39.x86_64.img \
    6.7.4-200.fc39.x86_64

# Debian:
update-initramfs -u -k 6.5.0-44-generic

# 5. Exit and reboot
exit
sudo umount -R /mnt
sudo reboot
```

### Verificar integridad del initramfs

```bash
# Check the file format
file /boot/initramfs-$(uname -r).img
# Should show: ... cpio archive, or ASCII cpio archive (SVR4)

# Try to list contents (if this fails, image is corrupt)
lsinitrd /boot/initramfs-$(uname -r).img > /dev/null 2>&1 && \
    echo "OK" || echo "CORRUPT"

# Check if kernel module versions match
KVER=$(uname -r)
lsinitrd /boot/initramfs-$KVER.img | grep "\.ko" | head -1
# Should show path containing $KVER
```

---

## Errores comunes

### 1. Confundir dracut modules con kernel modules

```bash
# WRONG: trying to add a kernel module with --add
sudo dracut --force --add "nvme"
# "nvme" is not a dracut module name! --add is for dracut modules

# RIGHT: use --add-drivers for kernel modules
sudo dracut --force --add-drivers "nvme"

# And --add for dracut functionality modules
sudo dracut --force --add "crypt lvm"
```

- `--add` / `--omit`: dracut modules (crypt, lvm, plymouth, nfs)
- `--add-drivers` / `--omit-drivers`: kernel modules (nvme, ext4, dm_crypt)

### 2. Regenerar con la versión de kernel equivocada en rescue

Cuando estás en un live USB, `$(uname -r)` devuelve la versión del kernel del **live USB**, no la del sistema que estás reparando:

```bash
# In chroot from live USB:
uname -r
# 6.1.0-13-amd64    ← This is the LIVE USB kernel!

# You need to specify the TARGET kernel:
ls /lib/modules/
# 6.7.4-200.fc39.x86_64    ← This is what you want

dracut --force /boot/initramfs-6.7.4-200.fc39.x86_64.img \
    6.7.4-200.fc39.x86_64
```

### 3. No incluir soporte de LUKS al cifrar un sistema existente

Si añades LUKS a un sistema que antes no tenía cifrado, el initramfs existente no incluye `cryptsetup` ni `dm-crypt.ko`:

```bash
# BEFORE encrypting, make sure initramfs will include LUKS support:

# RHEL:
sudo dracut --force --add "crypt"

# Debian: install the package first
sudo apt install cryptsetup-initramfs
sudo update-initramfs -u
```

### 4. Olvidar --force en dracut

Sin `--force`, dracut **no sobrescribe** un initramfs existente:

```bash
# This does NOTHING if the file already exists:
sudo dracut /boot/initramfs-$(uname -r).img
# No error, no warning, just silently skips

# Always use --force when regenerating:
sudo dracut --force
```

### 5. No verificar que /boot tiene espacio suficiente

```bash
# Before regenerating:
df -h /boot
# If < 50 MB free, clean old kernels first:

# RHEL:
sudo dnf remove --oldinstallonly --setopt=installonly_limit=2
# Debian:
sudo apt autoremove --purge
```

---

## Cheatsheet

```bash
# ═══════════════════════════════════════════════
#  REGENERAR — RHEL/Fedora (dracut)
# ═══════════════════════════════════════════════
sudo dracut --force                          # Current kernel
sudo dracut --force <path> <version>         # Specific kernel
sudo dracut --regenerate-all --force         # All kernels
sudo dracut --force --no-hostonly            # Generic (portable)
sudo dracut --force --add "crypt lvm"        # Add dracut modules
sudo dracut --force --add-drivers "nvme"     # Add kernel modules
sudo dracut --force --omit "plymouth"        # Remove dracut module
sudo dracut --force --compress=zstd          # Set compression
sudo dracut --force --verbose                # Verbose output

# ═══════════════════════════════════════════════
#  REGENERAR — Debian/Ubuntu (initramfs-tools)
# ═══════════════════════════════════════════════
sudo update-initramfs -u                     # Current kernel
sudo update-initramfs -u -k <version>        # Specific kernel
sudo update-initramfs -u -k all              # All kernels
sudo update-initramfs -u -v                  # Verbose
sudo update-initramfs -c -k <version>        # Create new
sudo update-initramfs -d -k <version>        # Delete

# ═══════════════════════════════════════════════
#  INSPECCIONAR
# ═══════════════════════════════════════════════
# RHEL:
lsinitrd /boot/initramfs-$(uname -r).img             # Full listing
lsinitrd <img> | grep "\.ko"                          # Kernel modules
lsinitrd <img> | grep "^dracut modules"               # Dracut modules
lsinitrd <img> -f /etc/crypttab                       # View file
dracut --list-modules                                  # Available modules

# Debian:
lsinitramfs /boot/initrd.img-$(uname -r)              # Full listing
unmkinitramfs /boot/initrd.img-$(uname -r) /tmp/dir   # Extract

# ═══════════════════════════════════════════════
#  CONFIGURACIÓN PERSISTENTE
# ═══════════════════════════════════════════════
# RHEL:  /etc/dracut.conf.d/*.conf
# Debian: /etc/initramfs-tools/initramfs.conf
#         /etc/initramfs-tools/modules
#         /etc/initramfs-tools/conf.d/

# ═══════════════════════════════════════════════
#  TROUBLESHOOTING
# ═══════════════════════════════════════════════
# Kernel params for dracut debugging:
#   rd.shell          → Shell on failure
#   rd.debug          → Verbose logging
#   rd.break          → Stop before switch_root
#   rd.break=pre-mount → Stop before mounting root

# In dracut emergency shell:
journalctl                          # Full log
cat /run/initramfs/rdsosreport.txt  # Diagnostic report
blkid                               # Compare UUIDs
cat /proc/cmdline                   # Check root=

# Verify initramfs integrity:
file /boot/initramfs-$(uname -r).img
lsinitrd /boot/initramfs-$(uname -r).img > /dev/null && echo OK
```

---

## Ejercicios

### Ejercicio 1: Comparar dracut y initramfs-tools

```bash
# This exercise works on RHEL/Fedora. Adapt for Debian if needed.

# Step 1: identify your tool
which dracut 2>/dev/null && echo "dracut" || echo "not dracut"
which update-initramfs 2>/dev/null && echo "initramfs-tools" || echo "not initramfs-tools"

# Step 2: list available dracut modules
dracut --list-modules 2>/dev/null | sort

# Step 3: check which dracut modules are in your current initramfs
lsinitrd /boot/initramfs-$(uname -r).img 2>/dev/null | \
    grep "^dracut modules"

# Step 4: check configuration
cat /etc/dracut.conf 2>/dev/null
ls /etc/dracut.conf.d/ 2>/dev/null
cat /etc/dracut.conf.d/*.conf 2>/dev/null

# For Debian:
cat /etc/initramfs-tools/initramfs.conf 2>/dev/null
cat /etc/initramfs-tools/modules 2>/dev/null

# Step 5: check compression used
file /boot/initramfs-$(uname -r).img 2>/dev/null || \
    file /boot/initrd.img-$(uname -r) 2>/dev/null
# Look for: gzip, xz, Zstandard, LZ4, etc.

# Step 6: check if hostonly or generic
lsinitrd /boot/initramfs-$(uname -r).img 2>/dev/null | \
    grep -c "\.ko"
ls /lib/modules/$(uname -r)/kernel/drivers/ | wc -l
# Compare: few modules in initramfs = hostonly
```

**Preguntas:**
1. ¿Cuántos dracut modules están disponibles vs cuántos están en tu initramfs?
2. ¿Qué compresión usa tu initramfs?
3. ¿Tu configuración tiene customizaciones en `conf.d/` o usa defaults?

> **Pregunta de predicción**: si ejecutas `dracut --list-modules` y ves "crypt" en la lista pero luego `lsinitrd ... | grep "^dracut modules"` no muestra "crypt", ¿significa que tu sistema no soporta LUKS? ¿O que dracut decidió no incluirlo por otra razón?

### Ejercicio 2: Regenerar con diferentes opciones

```bash
# Step 1: record current initramfs info
echo "=== Current ==="
ls -lh /boot/initramfs-$(uname -r).img
lsinitrd /boot/initramfs-$(uname -r).img | grep -c "\.ko"

# Step 2: backup
sudo cp /boot/initramfs-$(uname -r).img \
    /boot/initramfs-$(uname -r).img.backup

# Step 3: generate hostonly (should be similar to current)
sudo dracut --force --hostonly /tmp/initramfs-hostonly.img $(uname -r)
echo "=== Hostonly ==="
ls -lh /tmp/initramfs-hostonly.img
lsinitrd /tmp/initramfs-hostonly.img | grep -c "\.ko"

# Step 4: generate generic
sudo dracut --force --no-hostonly /tmp/initramfs-generic.img $(uname -r)
echo "=== Generic ==="
ls -lh /tmp/initramfs-generic.img
lsinitrd /tmp/initramfs-generic.img | grep -c "\.ko"

# Step 5: generate without Plymouth
sudo dracut --force --omit "plymouth" /tmp/initramfs-noplymouth.img $(uname -r)
echo "=== No Plymouth ==="
ls -lh /tmp/initramfs-noplymouth.img
lsinitrd /tmp/initramfs-noplymouth.img | grep -c "\.ko"

# Step 6: compare compression
sudo dracut --force --compress=gzip /tmp/initramfs-gzip.img $(uname -r)
sudo dracut --force --compress=xz /tmp/initramfs-xz.img $(uname -r)
sudo dracut --force --compress=zstd /tmp/initramfs-zstd.img $(uname -r)
echo "=== Compression comparison ==="
ls -lh /tmp/initramfs-{gzip,xz,zstd}.img

# Step 7: restore backup
sudo cp /boot/initramfs-$(uname -r).img.backup \
    /boot/initramfs-$(uname -r).img
sudo rm /boot/initramfs-$(uname -r).img.backup

# Step 8: clean up
rm -f /tmp/initramfs-*.img
```

**Preguntas:**
1. ¿Cuántas veces más grande es generic vs hostonly?
2. ¿Cuánto reduce el tamaño omitir Plymouth?
3. ¿Cuál es la compresión más pequeña? ¿Y la más rápida de generar?

> **Pregunta de predicción**: si generas un initramfs genérico en una VM con virtio y luego copias ese initramfs a un servidor físico con discos NVMe, ¿arrancará? ¿Y si el initramfs fuera hostonly?

### Ejercicio 3: Simular un initramfs roto y reparar

> **Solo en VM con snapshot.**

```bash
# PREPARATION: take a VM snapshot!

# Step 1: backup initramfs
sudo cp /boot/initramfs-$(uname -r).img \
    /boot/initramfs-$(uname -r).img.rescue

# Step 2: generate an initramfs that OMITS your storage driver
# Find your storage driver first:
lsmod | grep -E "nvme|ahci|virtio_blk|sd_mod"
# Let's say it's virtio_blk

sudo dracut --force --omit-drivers "virtio_blk virtio_scsi" \
    /boot/initramfs-$(uname -r).img $(uname -r)

# Step 3: verify the driver is missing
lsinitrd /boot/initramfs-$(uname -r).img | grep virtio
# Should show no virtio_blk module

# Step 4: reboot (system should fail to find root)
# sudo reboot

# EXPECTED: dracut emergency shell or timeout
# You'll see: "Warning: /dev/disk/by-uuid/... does not exist"

# REPAIR from dracut shell:
# dracut:/#
# Option A: if you have the backup:
#   (not easily accessible from dracut shell)
# Option B: boot from another kernel with working initramfs
#   (select older kernel in GRUB menu)

# REPAIR from live USB or working kernel:
# 1. Boot from GRUB menu: select a different kernel
# 2. Or boot from live USB, mount, chroot
# 3. Restore backup:
sudo cp /boot/initramfs-$(uname -r).img.rescue \
    /boot/initramfs-$(uname -r).img
# 4. Or regenerate properly:
sudo dracut --force

# Step 5: verify fix
sudo reboot
# System should boot normally

# Step 6: clean up backup
sudo rm -f /boot/initramfs-$(uname -r).img.rescue
```

**Preguntas:**
1. ¿Cuál fue el mensaje de error exacto cuando faltó el driver?
2. ¿Cuánto tiempo esperó dracut antes de dar la shell de emergencia?
3. ¿Pudiste cargar el módulo manualmente desde la shell de dracut?

> **Pregunta de predicción**: si en lugar de omitir el driver de disco, omites el módulo del filesystem (`ext4` o `xfs`), ¿el error será diferente? ¿El sistema encontrará el disco pero no podrá montarlo, o fallará de la misma forma?

---

> **Siguiente capítulo**: C05 — Kernel, S01 — Módulos del Kernel, T01 — lsmod, modprobe, modinfo.
