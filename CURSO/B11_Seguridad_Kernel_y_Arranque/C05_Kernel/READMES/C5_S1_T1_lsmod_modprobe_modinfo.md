# lsmod, modprobe, modinfo

## Índice
1. [Módulos del kernel: concepto](#módulos-del-kernel-concepto)
2. [Dónde viven los módulos](#dónde-viven-los-módulos)
3. [lsmod: listar módulos cargados](#lsmod-listar-módulos-cargados)
4. [modinfo: información de un módulo](#modinfo-información-de-un-módulo)
5. [modprobe: cargar y descargar módulos](#modprobe-cargar-y-descargar-módulos)
6. [insmod y rmmod: las herramientas de bajo nivel](#insmod-y-rmmod-las-herramientas-de-bajo-nivel)
7. [Carga automática de módulos](#carga-automática-de-módulos)
8. [Parámetros de módulos](#parámetros-de-módulos)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## Módulos del kernel: concepto

El kernel Linux tiene dos formas de incluir funcionalidad:

```
  ┌──────────────────────────────────────────────────────────────┐
  │                                                              │
  │  BUILT-IN (=y en .config)                                    │
  │  ┌────────────────────────────────────────────────────────┐  │
  │  │ Compilado directamente en vmlinuz                      │  │
  │  │ • Siempre presente en memoria                          │  │
  │  │ • No se puede descargar                                │  │
  │  │ • Necesario para arrancar (scheduler, VFS, tmpfs)      │  │
  │  │ • Aumenta el tamaño del kernel                         │  │
  │  └────────────────────────────────────────────────────────┘  │
  │                                                              │
  │  MODULE (=m en .config)                                      │
  │  ┌────────────────────────────────────────────────────────┐  │
  │  │ Compilado como archivo .ko separado                    │  │
  │  │ • Se carga bajo demanda (o manualmente)                │  │
  │  │ • Se puede descargar cuando no se usa                  │  │
  │  │ • Permite que un solo kernel soporte miles de devices   │  │
  │  │ • Ahorra RAM (solo carga lo necesario)                 │  │
  │  └────────────────────────────────────────────────────────┘  │
  │                                                              │
  │  NOT SET (is not set en .config)                             │
  │  ┌────────────────────────────────────────────────────────┐  │
  │  │ No compilado — funcionalidad ausente                   │  │
  │  │ • Para habilitar: recompilar el kernel                 │  │
  │  └────────────────────────────────────────────────────────┘  │
  │                                                              │
  └──────────────────────────────────────────────────────────────┘
```

Las distribuciones compilan la mayoría de drivers como módulos (`=m`) para que un solo kernel sirva en cualquier hardware. Solo lo esencial para el arranque temprano se compila built-in (`=y`).

### Tipos de módulos

| Categoría | Ejemplos | Función |
|---|---|---|
| **Storage drivers** | `ahci`, `nvme`, `sd_mod`, `virtio_blk` | Acceso a discos |
| **Filesystem** | `ext4`, `xfs`, `btrfs`, `nfs`, `vfat` | Leer/escribir filesystems |
| **Network drivers** | `e1000e`, `bnxt_en`, `iwlwifi`, `virtio_net` | Acceso a red |
| **GPU drivers** | `i915`, `amdgpu`, `nouveau`, `nvidia` | Vídeo |
| **USB** | `usbhid`, `usb_storage`, `xhci_hcd` | Dispositivos USB |
| **Sound** | `snd_hda_intel`, `snd_usb_audio` | Audio |
| **Crypto** | `aesni_intel`, `sha256_generic` | Criptografía |
| **Netfilter** | `nf_tables`, `nf_conntrack`, `iptable_filter` | Firewall |
| **Device Mapper** | `dm_mod`, `dm_crypt`, `dm_mirror` | LVM, LUKS |

---

## Dónde viven los módulos

```
  /lib/modules/
  └── $(uname -r)/                     ← Versión del kernel
      ├── kernel/                       ← Módulos organizados por subsistema
      │   ├── arch/                     ← Específicos de arquitectura
      │   ├── crypto/                   ← Algoritmos criptográficos
      │   ├── drivers/                  ← Drivers de hardware
      │   │   ├── ata/                  ← SATA (ahci.ko)
      │   │   ├── block/               ← Block devices
      │   │   ├── gpu/drm/             ← GPU (i915, amdgpu)
      │   │   ├── net/                  ← Red
      │   │   │   ├── ethernet/        ← Ethernet (e1000e, bnxt_en)
      │   │   │   └── wireless/        ← WiFi (iwlwifi)
      │   │   ├── nvme/                ← NVMe
      │   │   ├── scsi/                ← SCSI (sd_mod)
      │   │   ├── usb/                 ← USB
      │   │   └── ...
      │   ├── fs/                       ← Filesystems (ext4, xfs)
      │   ├── lib/                      ← Bibliotecas del kernel
      │   ├── net/                      ← Networking stack
      │   │   └── netfilter/           ← Firewall (nf_tables)
      │   └── sound/                    ← Audio
      ├── modules.dep                   ← Dependencias entre módulos
      ├── modules.dep.bin               ← Versión binaria (más rápida)
      ├── modules.alias                 ← Alias (PCI ID → módulo)
      ├── modules.alias.bin
      ├── modules.symbols               ← Símbolos exportados
      ├── modules.builtin               ← Lista de built-in modules
      ├── modules.builtin.modinfo       ← Metadatos de built-in
      └── modules.order                 ← Orden de carga preferido
```

```bash
# See your kernel version
uname -r
# 6.7.4-200.fc39.x86_64

# Count total available modules
find /lib/modules/$(uname -r)/kernel -name "*.ko*" | wc -l
# ~4000-6000 depending on distro

# Module file extension
ls /lib/modules/$(uname -r)/kernel/drivers/ata/
# ahci.ko.xz    ← .ko compressed with xz
# ahci.ko.zst   ← .ko compressed with zstd (Fedora 38+)

# .ko = kernel object (ELF shared object)
# .ko.xz / .ko.zst / .ko.gz = compressed kernel object
```

### Versionado estricto

Los módulos están **vinculados a la versión exacta del kernel**. Un módulo compilado para `6.7.4-200.fc39.x86_64` **no carga** en `6.7.4-201.fc39.x86_64`:

```bash
# This will fail:
insmod /lib/modules/6.7.4-200.fc39.x86_64/kernel/drivers/net/ethernet/intel/e1000e/e1000e.ko
# insmod: ERROR: could not insert module: Invalid module format
# (if running kernel is a different version)

# modprobe handles this correctly — only looks at current kernel's modules
modprobe e1000e   # Always uses /lib/modules/$(uname -r)/
```

---

## lsmod: listar módulos cargados

`lsmod` muestra los módulos actualmente cargados en el kernel.

### Uso básico

```bash
lsmod
# Module                  Size  Used by
# snd_hda_codec_realtek  155648  1
# snd_hda_codec_generic   98304  1 snd_hda_codec_realtek
# snd_hda_intel           57344  0
# snd_hda_codec          172032  3 snd_hda_codec_generic,snd_hda_intel,snd_hda_codec_realtek
# snd_hda_core           106496  4 snd_hda_codec_generic,...
# snd_pcm                143360  3 snd_hda_intel,snd_hda_codec,snd_hda_core
# nvidia               39845888  462
# nvidia_modeset         1286144  12 nvidia_drm
# ext4                   999424  1
# dm_crypt                57344  1
# ...
```

### Columnas

| Columna | Significado |
|---|---|
| **Module** | Nombre del módulo (sin `.ko`) |
| **Size** | Tamaño en bytes que ocupa en RAM |
| **Used by** | Número de dependientes + lista de módulos que lo usan |

### Filtrar la salida

```bash
# Find a specific module
lsmod | grep nvidia
# nvidia               39845888  462
# nvidia_modeset         1286144  12 nvidia_drm
# nvidia_drm              77824   8
# nvidia_uvm            3538944   0

# Count loaded modules
lsmod | wc -l
# ~120-200 on a typical system

# Find all network-related modules
lsmod | grep -iE "net|eth|wifi|wl|iwl"

# Find storage modules
lsmod | grep -iE "nvme|ahci|sd_mod|scsi|dm_"

# Sort by size (largest first)
lsmod | sort -k2 -n -r | head -10
```

### Lo que lsmod realmente lee

`lsmod` es simplemente un formateador de `/proc/modules`:

```bash
cat /proc/modules | head -5
# nvidia 39845888 462 - Live 0xffffffffc1000000 (POE)
# nvidia_modeset 1286144 12 nvidia_drm, Live 0xffffffffc0e00000 (POE)
# ...

# Fields: name size refcount dependencies state address (taint_flags)
# Taint flags: (P)roprietary, (O)ut-of-tree, (E)unsigned
```

---

## modinfo: información de un módulo

`modinfo` muestra metadatos de un módulo: qué hardware soporta, quién lo escribió, qué parámetros acepta.

### Uso básico

```bash
modinfo ext4
# filename:       /lib/modules/6.7.4-200.fc39.x86_64/kernel/fs/ext4/ext4.ko.xz
# softdep:        pre: crc32c
# license:        GPL
# description:    Fourth Extended Filesystem
# author:         Remy Card, Stephen Tweedie, Andrew Morton, ...
# alias:          fs-ext4
# alias:          ext3
# alias:          fs-ext3
# depends:        mbcache,jbd2
# retpoline:      Y
# intree:         Y
# name:           ext4
# vermagic:       6.7.4-200.fc39.x86_64 SMP preempt mod_unload
# sig_id:         PKCS#7
# signer:         Fedora kernel signing key
# ...
# parm:           mb_debug_v2:... (uint)
```

### Campos importantes

| Campo | Significado |
|---|---|
| `filename` | Ruta al archivo `.ko` |
| `license` | Licencia (GPL, proprietary, dual) |
| `description` | Descripción breve |
| `author` | Autor(es) |
| `alias` | Nombres alternativos y IDs de hardware soportados |
| `depends` | Módulos de los que depende (modprobe los carga automáticamente) |
| `softdep` | Dependencias opcionales |
| `vermagic` | Versión exacta del kernel para la que fue compilado |
| `intree` | `Y` = incluido en el kernel oficial, `N` = out-of-tree |
| `sig_id` | Tipo de firma (para Secure Boot) |
| `signer` | Quién firmó el módulo |
| `parm` | **Parámetros configurables** (ver sección dedicada) |

### Consultas específicas

```bash
# Show only the filename
modinfo -F filename ext4
# /lib/modules/6.7.4-200.fc39.x86_64/kernel/fs/ext4/ext4.ko.xz

# Show only parameters
modinfo -F parm ext4
# mb_debug_v2:... (uint)

# Show only dependencies
modinfo -F depends nvme
# nvme-core

# Show only description
modinfo -F description e1000e
# Intel(R) PRO/1000 Network Driver

# Show aliases (hardware IDs this module supports)
modinfo -F alias e1000e | head -5
# pci:v00008086d000015F3sv*sd*bc*sc*i*
# pci:v00008086d00001570sv*sd*bc*sc*i*
# ...
# These are PCI vendor:device IDs → used by udev for auto-loading

# Check a module for a different kernel version
modinfo -k 6.6.9-200.fc39.x86_64 ext4
```

### ¿Built-in o módulo?

```bash
# Check if a feature is built-in or module
cat /lib/modules/$(uname -r)/modules.builtin | grep ext4
# (empty = not built-in, i.e., it's a module or absent)

cat /lib/modules/$(uname -r)/modules.builtin | grep tmpfs
# kernel/fs/ramfs/ramfs.ko   ← built-in (but appears here)

# For running kernel, check /sys
cat /sys/module/ext4/initstate 2>/dev/null
# live     ← loaded as module
# (file doesn't exist if not loaded)

# Built-in modules appear in /sys/module/ but without initstate
ls /sys/module/vfs 2>/dev/null && echo "exists"
# Built-in features may or may not have /sys/module entries
```

---

## modprobe: cargar y descargar módulos

`modprobe` es la herramienta principal para gestionar módulos. A diferencia de `insmod`/`rmmod`, resuelve dependencias automáticamente.

### Cargar un módulo

```bash
# Load a module (resolves dependencies automatically)
sudo modprobe ext4
# This also loads: jbd2, mbcache, crc32c_generic (dependencies)

# Load with a parameter
sudo modprobe bonding mode=4 miimon=100

# Load and show what it does (dry run)
modprobe -n -v ext4
# insmod /lib/modules/.../kernel/lib/crc32c_generic.ko.xz
# insmod /lib/modules/.../kernel/fs/jbd2/jbd2.ko.xz
# insmod /lib/modules/.../kernel/fs/mbcache/mbcache.ko.xz
# insmod /lib/modules/.../kernel/fs/ext4/ext4.ko.xz

# -n = dry run (don't actually load)
# -v = verbose (show commands)
```

### Descargar un módulo

```bash
# Unload a module
sudo modprobe -r snd_hda_intel

# Unload also removes dependencies that are no longer needed
sudo modprobe -r ext4
# Also removes: jbd2, mbcache (if nothing else uses them)

# Dry run to see what would be removed
modprobe -r -n -v ext4

# Force remove (DANGEROUS — can crash system)
sudo modprobe -r --force ext4
# Only use if module is stuck and you understand the consequences
```

### Cargar falla: ¿por qué?

```bash
# Module not found
sudo modprobe nonexistent_module
# modprobe: FATAL: Module nonexistent_module not found in directory /lib/modules/6.7.4

# Module already loaded (not an error — silently succeeds)
sudo modprobe ext4
# (no output, no error)

# Module blocked by Secure Boot
sudo modprobe vboxdrv
# modprobe: ERROR: could not insert 'vboxdrv': Required key not available
# → Module is not signed or signature not in MOK/db

# Module blacklisted
sudo modprobe nouveau
# modprobe: FATAL: Module nouveau is in blacklist
# (if blacklisted in /etc/modprobe.d/)

# Module depends on missing module
sudo modprobe some_module
# modprobe: FATAL: Error inserting some_dep: Unknown symbol in module
```

### Descargar falla: ¿por qué?

```bash
# Module is in use
sudo modprobe -r ext4
# modprobe: FATAL: Module ext4 is in use.
# (a filesystem is mounted with ext4)

# Check what's using a module
lsmod | grep ext4
# ext4   999424  1
#                ^ "1" means one user (mounted filesystem)

# Module has dependents
sudo modprobe -r snd_hda_core
# modprobe: FATAL: Module snd_hda_core is in use.
lsmod | grep snd_hda_core
# snd_hda_core  106496  4 snd_hda_codec_generic,snd_hda_intel,...
#                        ^ 4 modules depend on it
```

---

## insmod y rmmod: las herramientas de bajo nivel

`insmod` y `rmmod` son las herramientas primitivas que `modprobe` usa internamente. Normalmente no las usas directamente.

```
  ┌────────────────────────────────────────────────────────────┐
  │                                                            │
  │  modprobe (alto nivel — usar siempre)                      │
  │  ├── Lee modules.dep para resolver dependencias            │
  │  ├── Lee /etc/modprobe.d/ para blacklists y opciones       │
  │  ├── Carga dependencias en orden correcto                  │
  │  ├── Aplica parámetros de /etc/modprobe.d/                 │
  │  └── Internamente ejecuta insmod para cada módulo          │
  │                                                            │
  │  insmod (bajo nivel — rara vez necesario)                  │
  │  ├── Carga UN solo módulo, especificando ruta completa     │
  │  ├── NO resuelve dependencias                              │
  │  ├── NO lee /etc/modprobe.d/                               │
  │  └── Falla si las dependencias no están cargadas           │
  │                                                            │
  │  rmmod (bajo nivel — rara vez necesario)                   │
  │  ├── Descarga UN solo módulo                               │
  │  ├── NO descarga dependencias automáticamente              │
  │  └── Falla si el módulo está en uso                        │
  │                                                            │
  └────────────────────────────────────────────────────────────┘
```

```bash
# insmod requires the FULL path to the .ko file
sudo insmod /lib/modules/$(uname -r)/kernel/fs/ext4/ext4.ko.xz
# ERROR: could not insert module: Unknown symbol in module
# (because jbd2 and mbcache aren't loaded yet)

# You'd have to load dependencies manually in order:
sudo insmod /lib/modules/$(uname -r)/kernel/fs/jbd2/jbd2.ko.xz
sudo insmod /lib/modules/$(uname -r)/kernel/fs/mbcache/mbcache.ko.xz
sudo insmod /lib/modules/$(uname -r)/kernel/fs/ext4/ext4.ko.xz
# Now it works — but modprobe does all this automatically

# rmmod
sudo rmmod ext4
# Just removes ext4, leaves jbd2 and mbcache loaded
```

### Cuándo usar insmod

- Cargar un módulo `.ko` compilado manualmente que no está en `/lib/modules/`
- Debugging: cargar un módulo desde una ruta específica
- Cuando `modprobe` no lo encuentra (módulo fuera del árbol)

---

## Carga automática de módulos

En uso normal, rara vez ejecutas `modprobe` manualmente. Los módulos se cargan automáticamente mediante **udev** y el sistema de **aliases**.

### Flujo de carga automática

```
  ┌──────────────────────────────────────────────────────────────┐
  │                 CARGA AUTOMÁTICA DE MÓDULOS                  │
  │                                                              │
  │  1. Hardware conectado (o detectado durante boot)            │
  │     ej: NIC Intel con PCI ID 8086:15f3                      │
  │                                                              │
  │  2. Kernel detecta el dispositivo PCI                        │
  │     → Genera un uevent con modalias:                         │
  │       "pci:v00008086d000015F3sv*sd*bc02sc00i*"               │
  │                                                              │
  │  3. udev recibe el uevent                                   │
  │     → Lee el modalias                                        │
  │     → Ejecuta: modprobe pci:v00008086d000015F3sv*sd*bc02sc00i│
  │                                                              │
  │  4. modprobe busca en modules.alias                          │
  │     → "pci:v00008086d000015F3sv*..." → e1000e                │
  │                                                              │
  │  5. modprobe carga e1000e (y sus dependencias)               │
  │                                                              │
  │  6. Driver inicializa el hardware                            │
  │     → Interface de red aparece (ej: enp3s0)                  │
  └──────────────────────────────────────────────────────────────┘
```

### modules.alias: el mapeo hardware → módulo

```bash
# View aliases for a specific module
grep e1000e /lib/modules/$(uname -r)/modules.alias | head -3
# alias pci:v00008086d000015F3sv*sd*bc*sc*i* e1000e
# alias pci:v00008086d00001570sv*sd*bc*sc*i* e1000e

# What module would load for a specific PCI device?
# Find the PCI ID of your device:
lspci -nn | grep -i net
# 03:00.0 Ethernet controller [0200]: Intel Corporation ... [8086:15f3]

# Check which module handles it
modprobe --resolve-alias "pci:v00008086d000015F3sv*sd*bc02sc00i00"
# e1000e
```

### Modalias: la cadena de identificación

Cada dispositivo genera un **modalias** — una cadena que identifica el tipo de hardware:

```bash
# View modalias of all PCI devices
cat /sys/bus/pci/devices/*/modalias | head -5
# pci:v00008086d000015F3sv0000103Csd00008783bc02sc00i00
# pci:v00008086d0000A0EFsv00008086sd00004070bc08sc80i00

# View modalias of USB devices
cat /sys/bus/usb/devices/*/modalias 2>/dev/null | head -5

# For a specific device
cat /sys/class/net/enp3s0/device/modalias
# pci:v00008086d000015F3sv0000103Csd00008783bc02sc00i00
```

---

## Parámetros de módulos

Muchos módulos aceptan **parámetros** que modifican su comportamiento.

### Ver parámetros disponibles

```bash
# List all parameters
modinfo -F parm e1000e
# IntMode:Interrupt Mode (array of int)
# SmartPowerDownEnable:Enable PHY smart power down (array of int)
# ...

# Or from sysfs (for loaded modules)
ls /sys/module/e1000e/parameters/ 2>/dev/null
# IntMode  SmartPowerDownEnable  ...

# View current value of a parameter
cat /sys/module/dm_crypt/parameters/max_read_size 2>/dev/null
```

### Pasar parámetros al cargar

```bash
# Via modprobe command line
sudo modprobe bonding mode=4 miimon=100

# Via /etc/modprobe.d/ (persistent, see next topic)
# options bonding mode=4 miimon=100

# Via kernel command line (in GRUB, for modules loaded during boot)
# e1000e.IntMode=1
```

### Cambiar parámetros en runtime

Algunos parámetros se pueden cambiar sin descargar el módulo:

```bash
# Check if parameter is writable
ls -la /sys/module/dm_crypt/parameters/
# -r--r--r-- max_read_size    ← read-only
# -rw-r--r-- max_write_size   ← writable!

# Change writable parameter
echo 131072 | sudo tee /sys/module/dm_crypt/parameters/max_write_size

# Read-only parameters require unload/reload:
sudo modprobe -r dm_crypt
sudo modprobe dm_crypt max_read_size=131072
```

---

## Errores comunes

### 1. Confundir nombre del módulo con nombre del archivo

Los módulos usan guiones bajos (`_`) en su nombre, pero los archivos pueden tener guiones (`-`):

```bash
# Module name uses underscore
lsmod | grep snd_hda_intel
# snd_hda_intel  57344  0

# But modprobe accepts both:
sudo modprobe snd-hda-intel    # Works (converted to _)
sudo modprobe snd_hda_intel    # Also works

# File on disk might be:
# snd-hda-intel.ko.xz  or  snd_hda_intel.ko.xz
```

### 2. Intentar descargar un módulo en uso

```bash
# This fails:
sudo modprobe -r ext4
# FATAL: Module ext4 is in use.

# Check what's using it:
lsmod | grep ext4
# ext4  999424  3
#               ^ 3 references (mounted filesystems)

# You must unmount all ext4 filesystems first
# (impossible for / if root is ext4)
```

### 3. Usar insmod cuando deberías usar modprobe

```bash
# WRONG: insmod won't resolve dependencies
sudo insmod /lib/modules/$(uname -r)/kernel/net/netfilter/nf_tables.ko.xz
# Unknown symbol in module

# RIGHT: modprobe resolves everything
sudo modprobe nf_tables
```

### 4. No entender que lsmod solo muestra módulos, no built-in

```bash
# "ext4 not in lsmod" does NOT mean "no ext4 support"
lsmod | grep tmpfs
# (empty — because tmpfs is built-in, not a module)

# But tmpfs works fine:
mount -t tmpfs tmpfs /mnt/test

# Check if something is built-in:
grep tmpfs /lib/modules/$(uname -r)/modules.builtin
# kernel/fs/ramfs/ramfs.ko    ← built-in
```

### 5. Ignorar los taint flags

Cuando un módulo propietario o out-of-tree se carga, el kernel se marca como **tainted** (contaminado). Esto afecta el soporte:

```bash
cat /proc/sys/kernel/tainted
# 0 = clean
# Non-zero = tainted

# Common taint flags:
# P (1)     = Proprietary module loaded (nvidia, etc.)
# O (4096)  = Out-of-tree module loaded
# E (32768) = Unsigned module loaded

# View which modules caused the taint
dmesg | grep -i taint
# nvidia: module license 'NVIDIA' taints kernel.
# nvidia: module verification failed: signature and/or required key missing
```

---

## Cheatsheet

```bash
# ═══════════════════════════════════════════════
#  LISTAR MÓDULOS
# ═══════════════════════════════════════════════
lsmod                               # All loaded modules
lsmod | grep <name>                 # Find specific module
lsmod | wc -l                       # Count loaded
lsmod | sort -k2 -n -r | head      # Largest modules
cat /proc/modules                   # Raw module info

# ═══════════════════════════════════════════════
#  INFORMACIÓN DE MÓDULOS
# ═══════════════════════════════════════════════
modinfo <module>                    # Full info
modinfo -F filename <module>        # File path
modinfo -F depends <module>         # Dependencies
modinfo -F parm <module>            # Parameters
modinfo -F description <module>     # Description
modinfo -F alias <module>           # Hardware aliases
modinfo -k <version> <module>       # Info for other kernel

# ═══════════════════════════════════════════════
#  CARGAR / DESCARGAR
# ═══════════════════════════════════════════════
sudo modprobe <module>              # Load (with dependencies)
sudo modprobe <module> param=value  # Load with parameters
sudo modprobe -r <module>           # Unload (with unused deps)
modprobe -n -v <module>             # Dry run (show what would load)
modprobe --resolve-alias "alias"    # Find module for alias

# ═══════════════════════════════════════════════
#  BAJO NIVEL (rara vez necesario)
# ═══════════════════════════════════════════════
sudo insmod /path/to/module.ko      # Load by path (no deps)
sudo rmmod <module>                 # Unload (no dep cleanup)

# ═══════════════════════════════════════════════
#  INTROSPECCIÓN
# ═══════════════════════════════════════════════
uname -r                            # Current kernel version
ls /lib/modules/$(uname -r)/kernel/ # Module tree
find /lib/modules/$(uname -r) -name "*.ko*" | wc -l  # Total modules
cat /lib/modules/$(uname -r)/modules.builtin | wc -l  # Built-in count
ls /sys/module/<name>/parameters/   # Runtime parameters
cat /sys/module/<name>/parameters/<p>  # Parameter value
cat /proc/sys/kernel/tainted        # Taint status
```

---

## Ejercicios

### Ejercicio 1: Explorar los módulos de tu sistema

```bash
# Step 1: list loaded modules and count them
lsmod | wc -l
echo "---"

# Step 2: find the 5 largest modules by size
lsmod | sort -k2 -n -r | head -5

# Step 3: count total available modules
find /lib/modules/$(uname -r)/kernel -name "*.ko*" | wc -l

# Step 4: compare loaded vs available
echo "Loaded: $(lsmod | tail -n +2 | wc -l)"
echo "Available: $(find /lib/modules/$(uname -r)/kernel -name '*.ko*' | wc -l)"

# Step 5: identify your storage driver
lsmod | grep -iE "nvme|ahci|virtio_blk|sd_mod"

# Step 6: identify your network driver
IFACE=$(ip route | grep default | awk '{print $5}')
echo "Default interface: $IFACE"
basename $(readlink /sys/class/net/$IFACE/device/driver/module) 2>/dev/null

# Step 7: identify your filesystem modules
lsmod | grep -iE "ext4|xfs|btrfs"

# Step 8: check for tainted kernel
TAINT=$(cat /proc/sys/kernel/tainted)
echo "Taint value: $TAINT"
[ "$TAINT" -eq 0 ] && echo "Clean" || echo "Tainted (check dmesg | grep taint)"
```

**Preguntas:**
1. ¿Qué porcentaje de los módulos disponibles está cargado? ¿Por qué es tan bajo?
2. ¿Cuál es el módulo más grande? ¿Por qué crees que ocupa tanto?
3. ¿Tu kernel está tainted? Si sí, ¿qué módulo lo causa?

> **Pregunta de predicción**: si ejecutas `lsmod | grep tmpfs` y no aparece nada, ¿significa que tu sistema no soporta tmpfs? Verifica con `mount | grep tmpfs` y explica la discrepancia.

### Ejercicio 2: modinfo y dependencias

```bash
# Step 1: get full info on your storage driver
STORAGE=$(lsmod | grep -oE "nvme|ahci|virtio_blk" | head -1)
echo "Storage driver: $STORAGE"
modinfo $STORAGE

# Step 2: trace the dependency chain
echo "=== Dependencies of $STORAGE ==="
modinfo -F depends $STORAGE
# For each dependency, check ITS dependencies:
for dep in $(modinfo -F depends $STORAGE | tr ',' ' '); do
    echo "  $dep depends on: $(modinfo -F depends $dep 2>/dev/null)"
done

# Step 3: check parameters of ext4 (or xfs)
echo "=== ext4 parameters ==="
modinfo -F parm ext4

# Step 4: view current values of loaded module parameters
MODULE="ext4"  # or xfs, or your storage driver
if [ -d /sys/module/$MODULE/parameters ]; then
    echo "=== Current parameter values for $MODULE ==="
    for p in /sys/module/$MODULE/parameters/*; do
        echo "  $(basename $p) = $(cat $p 2>/dev/null)"
    done
fi

# Step 5: check if a specific feature is built-in or module
for feat in ext4 xfs tmpfs vfat nfs btrfs; do
    if grep -q "/$feat/" /lib/modules/$(uname -r)/modules.builtin 2>/dev/null; then
        echo "$feat: built-in"
    elif modinfo $feat &>/dev/null; then
        if lsmod | grep -q "^$feat "; then
            echo "$feat: module (loaded)"
        else
            echo "$feat: module (not loaded)"
        fi
    else
        echo "$feat: not available"
    fi
done
```

**Preguntas:**
1. ¿Cuántas dependencias tiene tu driver de almacenamiento?
2. ¿`ext4` tiene parámetros configurables? ¿Cuáles podrían ser útiles para un sysadmin?
3. De los filesystems comprobados, ¿cuáles son built-in, cuáles módulo, y cuáles no disponibles?

> **Pregunta de predicción**: si ejecutas `modprobe -n -v ext4`, ¿cuántos comandos `insmod` mostrará? ¿Coincide con la lista de dependencias de `modinfo -F depends ext4`?

### Ejercicio 3: Cargar y descargar módulos

```bash
# Step 1: find a module that is available but NOT loaded
# (vfat is a good candidate — unless you have FAT partitions mounted)
lsmod | grep vfat
# Should be empty (or pick another unused module)

# Step 2: dry run — what would modprobe do?
modprobe -n -v vfat

# Step 3: load it
sudo modprobe vfat
lsmod | grep vfat
# vfat    24576  0
# fat     86016  1 vfat    ← dependency loaded automatically

# Step 4: check module info while loaded
ls /sys/module/vfat/
cat /sys/module/vfat/initstate
# live

# Step 5: unload it
sudo modprobe -r vfat
lsmod | grep vfat
# (should be empty)
lsmod | grep fat
# (fat should also be removed if nothing else uses it)

# Step 6: try to unload a module that's in use
lsmod | grep ext4
# ext4  999424  3   ← "3" references = in use
sudo modprobe -r ext4
# FATAL: Module ext4 is in use.

# Step 7: test insmod vs modprobe (understand the difference)
# Try loading vfat with insmod (no dependencies):
VFAT_PATH=$(modinfo -F filename vfat)
sudo insmod $VFAT_PATH
# ERROR: Unknown symbol (because "fat" module isn't loaded)

# Now load fat first, then vfat:
FAT_PATH=$(modinfo -F filename fat)
sudo insmod $FAT_PATH
sudo insmod $VFAT_PATH
# Success!

# Clean up:
sudo modprobe -r vfat
```

**Preguntas:**
1. ¿Cuántos módulos cargó `modprobe vfat` en total (incluyendo dependencias)?
2. ¿`modprobe -r vfat` también descargó las dependencias? ¿Por qué?
3. ¿Qué error dio `insmod` sin cargar la dependencia `fat` primero?

> **Pregunta de predicción**: si cargas `vfat` con `modprobe`, luego montas un USB con FAT32, e intentas `modprobe -r vfat`, ¿qué pasará? ¿Y si desmontas el USB y luego intentas descargar?

---

> **Siguiente tema**: T02 — /etc/modprobe.d/: blacklisting, opciones de módulo, aliases.
