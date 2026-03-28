# Compilación del Kernel

## Índice
1. [¿Por qué compilar un kernel?](#por-qué-compilar-un-kernel)
2. [Obtener el código fuente](#obtener-el-código-fuente)
3. [El archivo .config](#el-archivo-config)
4. [Herramientas de configuración](#herramientas-de-configuración)
5. [Compilación paso a paso](#compilación-paso-a-paso)
6. [Instalación](#instalación)
7. [Verificar el nuevo kernel](#verificar-el-nuevo-kernel)
8. [Desinstalar un kernel compilado](#desinstalar-un-kernel-compilado)
9. [Opciones de configuración importantes](#opciones-de-configuración-importantes)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## ¿Por qué compilar un kernel?

Las distribuciones proveen kernels genéricos que funcionan en la mayoría del hardware. Compilar tu propio kernel tiene sentido en situaciones específicas:

| Razón | Ejemplo |
|---|---|
| **Habilitar funcionalidad ausente** | Un filesystem o driver no incluido en el kernel de tu distro |
| **Optimizar para hardware específico** | Eliminar miles de drivers innecesarios, reducir tamaño |
| **Aplicar parches** | Parches de seguridad urgentes, parches de rendimiento |
| **Aprender** | Entender la estructura interna del kernel |
| **Embedded/IoT** | Kernel mínimo para un dispositivo con recursos limitados |
| **Requisito del trabajo** | Algunas empresas mantienen kernels personalizados |

```
  ┌────────────────────────────────────────────────────────────┐
  │  Kernel de distribución     vs     Kernel personalizado    │
  │                                                            │
  │  • ~4000-6000 módulos              • Solo lo que necesitas │
  │  • ~30 MB vmlinuz                  • Puede ser < 5 MB     │
  │  • Funciona en todo                • Solo en tu hardware   │
  │  • Soporte del vendor              • Tú lo mantienes      │
  │  • Actualizaciones automáticas     • Actualizas manualmente│
  └────────────────────────────────────────────────────────────┘
```

> **En producción**: salvo requisitos muy específicos, usa el kernel de tu distribución. Compilar un kernel es un ejercicio educativo valioso pero una carga operacional en producción.

---

## Obtener el código fuente

### Desde kernel.org

```bash
# Check latest versions
# https://www.kernel.org/

# Download a specific version
cd /usr/src
sudo wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.7.4.tar.xz

# Verify integrity (optional but recommended)
sudo wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.7.4.tar.sign
unxz linux-6.7.4.tar.xz
gpg --verify linux-6.7.4.tar.sign linux-6.7.4.tar

# Extract
sudo tar xf linux-6.7.4.tar -C /usr/src/
cd /usr/src/linux-6.7.4

# Create symlink (convention)
sudo ln -sf linux-6.7.4 /usr/src/linux
```

### Desde el paquete de tu distribución

```bash
# RHEL/Fedora: install kernel source RPM
sudo dnf install kernel-devel-$(uname -r)
# Headers and build infrastructure in /usr/src/kernels/$(uname -r)/

# Debian/Ubuntu: install kernel source package
sudo apt install linux-source-$(uname -r | cut -d- -f1)
# Tarball in /usr/src/linux-source-*.tar.bz2
```

### Requisitos de compilación

```bash
# RHEL/Fedora
sudo dnf groupinstall "Development Tools"
sudo dnf install ncurses-devel bison flex elfutils-libelf-devel \
    openssl-devel bc perl dwarves

# Debian/Ubuntu
sudo apt install build-essential libncurses-dev bison flex \
    libelf-dev libssl-dev bc perl dwarves

# Verify
make --version    # GNU Make
gcc --version     # GCC compiler
```

---

## El archivo .config

El archivo `.config` en la raíz del código fuente define **cada opción** del kernel: qué se compila built-in (`=y`), como módulo (`=m`), o se omite (`is not set`).

### Formato

```bash
head -20 .config
# # Automatically generated file; DO NOT EDIT.
# # Linux/x86 6.7.4 Kernel Configuration
# #
# CONFIG_CC_VERSION_TEXT="gcc (GCC) 13.2.1"
# CONFIG_CC_IS_GCC=y
# CONFIG_GCC_VERSION=130201
# ...
# CONFIG_64BIT=y
# CONFIG_X86_64=y
# ...
# CONFIG_MODULES=y        ← Support for loadable modules
# CONFIG_EXT4_FS=m         ← ext4 as module
# CONFIG_XFS_FS=y          ← XFS built-in
# # CONFIG_BTRFS_FS is not set  ← btrfs disabled
```

Tres estados posibles:

```
  CONFIG_FEATURE=y          → Built-in (compilado en vmlinuz)
  CONFIG_FEATURE=m          → Module (compilado como .ko)
  # CONFIG_FEATURE is not set  → Disabled (no compilado)
```

### Obtener un .config inicial

Nunca escribes `.config` desde cero — siempre partes de una base:

```bash
# ── Opción 1: copiar la config del kernel actual ──
cp /boot/config-$(uname -r) .config
# Best starting point: same options as your running kernel

# ── Opción 2: config de la distribución ──
# RHEL:
cp /boot/config-$(uname -r) .config
# Debian:
cp /boot/config-$(uname -r) .config

# ── Opción 3: config mínima por defecto ──
make defconfig
# Genera un .config mínimo funcional para tu arquitectura

# ── Opción 4: config basada en módulos cargados ──
make localmodconfig
# Genera .config con SOLO los módulos actualmente cargados
# Muy pequeño pero solo funciona en TU hardware exacto

# ── Opción 5: toda opción como módulo ──
make allmodconfig
# Todo lo posible como módulo (para testing)

# ── Opción 6: todo habilitado ──
make allyesconfig
# Todo built-in (kernel enorme, para testing)
```

### Actualizar .config para un kernel nuevo

Cuando usas un `.config` de una versión anterior en una versión nueva, hay opciones nuevas que no existían. `make oldconfig` pregunta interactivamente por cada opción nueva:

```bash
# Copy old config
cp /boot/config-$(uname -r) .config

# Answer questions about NEW options only
make oldconfig
# New option: CONFIG_NEW_FEATURE (NEW_FEATURE) [Y/m/n/?] (NEW)
# Press Enter for default, or choose y/m/n

# Non-interactive: accept defaults for all new options
make olddefconfig
# Same as oldconfig but automatically selects defaults
```

---

## Herramientas de configuración

### make menuconfig (TUI — recomendado)

```bash
make menuconfig
```

```
  ┌──────────────────────────────────────────────────────────┐
  │             Linux/x86 6.7.4 Kernel Configuration         │
  │                                                          │
  │  Arrow keys navigate the menu.  <Enter> selects          │
  │  submenus.  Highlighted letters are hotkeys.              │
  │  Pressing <Y> includes, <N> excludes, <M> modularizes.  │
  │                                                          │
  │  ┌──────────────────────────────────────────────────┐    │
  │  │    General setup  --->                           │    │
  │  │    Processor type and features  --->             │    │
  │  │    Power management and ACPI options  --->       │    │
  │  │    Bus options (PCI etc.)  --->                  │    │
  │  │    Binary Emulations  --->                       │    │
  │  │    Firmware Drivers  --->                        │    │
  │  │    [*] Virtualization  --->                      │    │
  │  │    General architecture-dependent options  --->  │    │
  │  │    [*] Enable loadable module support  --->      │    │
  │  │    -*- Enable the block layer  --->              │    │
  │  │    IO Schedulers  --->                           │    │
  │  │    Memory Management options  --->               │    │
  │  │    [*] Networking support  --->                  │    │
  │  │    Device Drivers  --->                          │    │
  │  │    File systems  --->                            │    │
  │  │    Security options  --->                        │    │
  │  │    -*- Cryptographic API  --->                   │    │
  │  │    Library routines  --->                        │    │
  │  │    Kernel hacking  --->                          │    │
  │  └──────────────────────────────────────────────────┘    │
  │                                                          │
  │           <Select>  < Exit >  < Help >  < Save >         │
  └──────────────────────────────────────────────────────────┘
```

Navegación:

| Tecla | Acción |
|---|---|
| `↑` / `↓` | Navegar |
| `Enter` | Entrar en submenú |
| `Y` | Built-in (`[*]` o `<*>`) |
| `M` | Module (`<M>`) |
| `N` | Disabled (`[ ]` o `< >`) |
| `/` | **Buscar** opción por nombre |
| `?` | Ayuda sobre la opción seleccionada |
| `Esc Esc` | Subir un nivel / Salir |

Símbolos:

```
  [*]  → Boolean: enabled (built-in)
  [ ]  → Boolean: disabled
  <*>  → Tristate: built-in
  <M>  → Tristate: module
  < >  → Tristate: disabled
  -*-  → Forced on (dependency of another option)
  -M-  → Forced module
  (value)  → String or number value
  --->     → Has submenu
```

### make nconfig (TUI alternativo)

```bash
make nconfig   # Similar a menuconfig pero con interfaz ncurses mejorada
```

### make xconfig (GUI con Qt)

```bash
# Requires Qt development libraries
sudo dnf install qt5-qtbase-devel   # Fedora
make xconfig
```

### make gconfig (GUI con GTK)

```bash
sudo dnf install gtk3-devel         # Fedora
make gconfig
```

### Buscar una opción

En `menuconfig`, presiona `/` para buscar:

```
  Search: EXT4

  Symbol: EXT4_FS [=m]
  Type  : tristate
  Prompt: The Extended 4 (ext4) filesystem
    Location:
      -> File systems
        -> The Extended 4 (ext4) filesystem (EXT4_FS [=m])
    Defined at fs/ext4/Kconfig:1
    Depends on: BLOCK [=y]
    Selects: FS_IOMAP [=y], JBD2 [=m], ...
    Selected by [n]:
      - ...
```

---

## Compilación paso a paso

### Flujo completo

```
  ┌──────────────────────────────────────────────────────────────┐
  │                                                              │
  │  1. Obtener fuente     tar xf linux-6.7.4.tar               │
  │          │                                                   │
  │          ▼                                                   │
  │  2. Configurar         make menuconfig  →  .config           │
  │          │                                                   │
  │          ▼                                                   │
  │  3. Compilar           make -j$(nproc)                       │
  │          │             vmlinuz + módulos                      │
  │          │             (5-60 min según CPU y config)          │
  │          ▼                                                   │
  │  4. Instalar módulos   make modules_install                  │
  │          │             → /lib/modules/6.7.4/                 │
  │          ▼                                                   │
  │  5. Instalar kernel    make install                          │
  │          │             → /boot/vmlinuz-6.7.4                 │
  │          │             → /boot/initramfs-6.7.4.img (auto)    │
  │          │             → actualizar GRUB                     │
  │          ▼                                                   │
  │  6. Reiniciar          reboot                                │
  │          │             Seleccionar nuevo kernel en GRUB       │
  │          ▼                                                   │
  │  7. Verificar          uname -r → 6.7.4                     │
  │                                                              │
  └──────────────────────────────────────────────────────────────┘
```

### Paso 1: Limpiar (opcional)

```bash
cd /usr/src/linux-6.7.4

# Clean everything (remove all generated files including .config)
make mrproper

# Clean compiled objects but keep .config
make clean

# Only clean if you're starting fresh or changing architecture
```

| Target | Limpia | Conserva .config |
|---|---|---|
| `make clean` | Objetos compilados (.o, .ko) | Sí |
| `make mrproper` | Todo (objetos + .config + backups) | No |
| `make distclean` | Todo + archivos de editores, patches | No |

### Paso 2: Configurar

```bash
# Start from current kernel config
cp /boot/config-$(uname -r) .config

# Update for new kernel version (accept defaults for new options)
make olddefconfig

# Optional: customize further
make menuconfig
```

### Paso 3: Compilar

```bash
# Compile kernel + modules (use all CPU cores)
make -j$(nproc)

# What this does:
# 1. Compiles all .c files into .o objects
# 2. Links vmlinux (uncompressed kernel)
# 3. Compresses to arch/x86/boot/bzImage (= vmlinuz)
# 4. Compiles all modules (.ko files)

# Time estimates (8-core modern CPU):
# defconfig:         ~5-10 minutes
# distro full config: ~30-60 minutes
# allmodconfig:       ~60-120 minutes
```

### Paso 4: Instalar módulos

```bash
sudo make modules_install

# This copies all .ko files to:
# /lib/modules/6.7.4/
# And runs depmod -a automatically

# Verify
ls /lib/modules/6.7.4/
# kernel/  modules.dep  modules.alias  ...
```

### Paso 5: Instalar kernel

```bash
sudo make install

# This:
# 1. Copies arch/x86/boot/bzImage → /boot/vmlinuz-6.7.4
# 2. Copies System.map → /boot/System.map-6.7.4
# 3. Copies .config → /boot/config-6.7.4
# 4. Runs dracut/update-initramfs to generate initramfs
# 5. Updates GRUB (grub2-mkconfig or update-grub)
```

### Personalizar la versión (LOCALVERSION)

Para distinguir tu kernel del de la distribución:

```bash
# Method 1: in .config
# CONFIG_LOCALVERSION="-custom"
# Result: uname -r → 6.7.4-custom

# Method 2: in menuconfig
# General setup → Local version - append to kernel release

# Method 3: command line
make LOCALVERSION="-custom" -j$(nproc)
```

---

## Instalación

### En RHEL/Fedora

```bash
# After make modules_install:
sudo make install
# Uses installkernel script which:
# 1. Copies vmlinuz and System.map to /boot/
# 2. Runs dracut to generate initramfs
# 3. Runs grubby to add BLS entry

# Verify
ls /boot/vmlinuz-6.7.4*
ls /boot/initramfs-6.7.4*
ls /boot/loader/entries/*6.7.4*    # BLS entry (Fedora/RHEL 8+)
```

### En Debian/Ubuntu

```bash
# After make modules_install:
sudo make install
# Uses installkernel which:
# 1. Copies vmlinuz and System.map to /boot/
# 2. Runs update-initramfs -c -k 6.7.4
# 3. Runs update-grub

# Verify
ls /boot/vmlinuz-6.7.4*
ls /boot/initrd.img-6.7.4*
grep "6.7.4" /boot/grub/grub.cfg
```

### Método alternativo: deb-pkg / rpm-pkg

En lugar de `make install`, puedes crear un **paquete** que se instala limpiamente con el gestor de paquetes:

```bash
# Create a .deb package (Debian/Ubuntu)
make -j$(nproc) deb-pkg
# Produces: ../linux-image-6.7.4-custom_6.7.4-1_amd64.deb
sudo dpkg -i ../linux-image-6.7.4-custom_*.deb

# Create an .rpm package (RHEL/Fedora)
make -j$(nproc) rpm-pkg
# Produces: ~/rpmbuild/RPMS/x86_64/kernel-6.7.4-*.rpm
sudo rpm -ivh ~/rpmbuild/RPMS/x86_64/kernel-6.7.4-*.rpm

# Advantages of packaging:
# • Clean uninstall with package manager
# • Proper dependency tracking
# • Can distribute to other machines
```

---

## Verificar el nuevo kernel

```bash
# 1. Reboot and select the new kernel in GRUB

# 2. Verify running kernel
uname -r
# 6.7.4-custom

# 3. Check kernel config
cat /proc/config.gz | gunzip | grep EXT4    # If CONFIG_IKCONFIG=y
# Or:
cat /boot/config-6.7.4-custom | grep EXT4

# 4. Verify modules
lsmod
ls /lib/modules/6.7.4-custom/

# 5. Check dmesg for errors
dmesg | grep -i error
dmesg | grep -i warning

# 6. Test specific features you enabled/disabled
```

---

## Desinstalar un kernel compilado

```bash
# If installed with make install:
sudo rm /boot/vmlinuz-6.7.4-custom
sudo rm /boot/System.map-6.7.4-custom
sudo rm /boot/config-6.7.4-custom
sudo rm /boot/initramfs-6.7.4-custom.img    # or initrd.img-*
sudo rm -rf /lib/modules/6.7.4-custom/

# Remove BLS entry (Fedora/RHEL 8+)
sudo rm /boot/loader/entries/*6.7.4-custom*

# Regenerate GRUB
sudo grub2-mkconfig -o /boot/grub2/grub.cfg    # RHEL
sudo update-grub                                 # Debian

# If installed with deb-pkg:
sudo dpkg -r linux-image-6.7.4-custom

# If installed with rpm-pkg:
sudo rpm -e kernel-6.7.4-custom
```

---

## Opciones de configuración importantes

### General

```
CONFIG_LOCALVERSION="-custom"        # Suffix for uname -r
CONFIG_MODULES=y                      # Enable loadable modules (essential)
CONFIG_MODULE_UNLOAD=y                # Allow module unloading
CONFIG_IKCONFIG=y                     # Save .config in kernel
CONFIG_IKCONFIG_PROC=y                # /proc/config.gz
```

### Seguridad

```
CONFIG_SECURITY_SELINUX=y             # SELinux support
CONFIG_SECURITY_APPARMOR=y            # AppArmor support
CONFIG_RANDOMIZE_BASE=y               # KASLR (kernel ASLR)
CONFIG_STACKPROTECTOR=y               # Stack canary
CONFIG_STACKPROTECTOR_STRONG=y        # Strong stack protection
CONFIG_FORTIFY_SOURCE=y               # Buffer overflow detection
CONFIG_MODULE_SIG=y                   # Module signature verification
CONFIG_MODULE_SIG_FORCE=y             # Require signed modules
CONFIG_LOCK_DOWN_KERNEL_FORCE_INTEGRITY=y  # Lockdown mode
```

### Filesystems

```
CONFIG_EXT4_FS=y                      # ext4 (built-in for root)
CONFIG_XFS_FS=m                       # XFS as module
CONFIG_BTRFS_FS=m                     # Btrfs as module
CONFIG_VFAT_FS=m                      # FAT (for ESP/USB)
CONFIG_FUSE_FS=m                      # FUSE (userspace filesystems)
CONFIG_OVERLAY_FS=m                   # OverlayFS (containers)
CONFIG_NFS_FS=m                       # NFS client
```

### Virtualización y contenedores

```
CONFIG_KVM=m                          # KVM
CONFIG_KVM_INTEL=m                    # KVM for Intel
CONFIG_KVM_AMD=m                      # KVM for AMD
CONFIG_VHOST_NET=m                    # Vhost-net for VM networking
CONFIG_NAMESPACES=y                   # Namespaces (containers)
CONFIG_CGROUPS=y                      # Control groups (containers)
CONFIG_OVERLAY_FS=m                   # OverlayFS (Docker)
```

### Networking

```
CONFIG_NETFILTER=y                    # Firewall framework
CONFIG_NF_TABLES=m                    # nftables
CONFIG_BRIDGE=m                       # Bridge (VMs, containers)
CONFIG_BONDING=m                      # Link aggregation
CONFIG_TUN=m                          # TUN/TAP (VPN)
CONFIG_VLAN_8021Q=m                   # 802.1Q VLANs
```

---

## Errores comunes

### 1. Compilar sin suficiente espacio en disco

```bash
# Kernel source + compilation needs 15-30 GB!
df -h /usr/src
# Make sure you have at least 20 GB free

# Clean after compilation to reclaim space:
make clean           # Remove .o files (~10-15 GB)
# Or keep only what's needed:
# The .ko files are in /lib/modules/ after modules_install
```

### 2. Olvidar make modules_install antes de make install

```bash
# WRONG order:
make -j$(nproc)
sudo make install          # Installs kernel but no modules!
sudo make modules_install  # Too late — initramfs already generated
                           # without the modules

# RIGHT order:
make -j$(nproc)
sudo make modules_install  # First: install modules
sudo make install          # Then: install kernel + generate initramfs
```

### 3. Deshabilitar algo que tu root filesystem necesita

```bash
# If root is on ext4 and you set CONFIG_EXT4_FS=m (module, not built-in):
# → Kernel can't mount root without initramfs
# → If initramfs doesn't include ext4.ko → kernel panic

# SAFE choices for root filesystem:
# CONFIG_EXT4_FS=y    (built-in, always available)
# CONFIG_EXT4_FS=m    (module, works IF initramfs includes it)

# DANGEROUS:
# # CONFIG_EXT4_FS is not set  → Can't mount root AT ALL
```

### 4. Usar make -j sin nproc (demasiados o pocos procesos)

```bash
# SLOW: only one core
make

# OPTIMAL: use all cores
make -j$(nproc)

# RISKY: too many jobs, system runs out of RAM
make -j128    # On a system with 4 cores / 8 GB RAM → OOM killer

# Rule of thumb: -j$(nproc) or -j$(($(nproc) + 1))
```

### 5. No guardar el .config

```bash
# After spending 30 minutes in menuconfig:
make mrproper    # OOPS — .config is GONE

# Always save a backup:
cp .config ../config-6.7.4-custom.bak

# Or use make install (copies .config to /boot/config-*)
# Or enable CONFIG_IKCONFIG_PROC=y for /proc/config.gz
```

---

## Cheatsheet

```bash
# ═══════════════════════════════════════════════
#  OBTENER FUENTE
# ═══════════════════════════════════════════════
wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.7.4.tar.xz
tar xf linux-6.7.4.tar.xz && cd linux-6.7.4

# ═══════════════════════════════════════════════
#  CONFIGURAR
# ═══════════════════════════════════════════════
cp /boot/config-$(uname -r) .config   # Start from current
make olddefconfig                       # Update for new version
make menuconfig                         # TUI configuration
make localmodconfig                     # Minimal (current HW only)
make defconfig                          # Architecture default

# ═══════════════════════════════════════════════
#  LIMPIAR
# ═══════════════════════════════════════════════
make clean                              # Remove .o (keep .config)
make mrproper                           # Remove ALL generated files
make distclean                          # mrproper + editor backups

# ═══════════════════════════════════════════════
#  COMPILAR
# ═══════════════════════════════════════════════
make -j$(nproc)                         # Compile kernel + modules
make -j$(nproc) LOCALVERSION="-custom"  # With custom suffix

# ═══════════════════════════════════════════════
#  INSTALAR
# ═══════════════════════════════════════════════
sudo make modules_install               # → /lib/modules/<version>/
sudo make install                       # → /boot/ + initramfs + GRUB

# ═══════════════════════════════════════════════
#  EMPAQUETAR (alternativa a make install)
# ═══════════════════════════════════════════════
make -j$(nproc) deb-pkg                 # Debian .deb package
make -j$(nproc) rpm-pkg                 # RHEL .rpm package

# ═══════════════════════════════════════════════
#  VERIFICAR
# ═══════════════════════════════════════════════
uname -r                                # Running kernel version
cat /proc/cmdline                       # Boot parameters
dmesg | grep -i error                   # Boot errors
ls /lib/modules/$(uname -r)/            # Installed modules

# ═══════════════════════════════════════════════
#  DESINSTALAR
# ═══════════════════════════════════════════════
sudo rm /boot/vmlinuz-<version>
sudo rm /boot/System.map-<version>
sudo rm /boot/config-<version>
sudo rm /boot/initramfs-<version>.img
sudo rm -rf /lib/modules/<version>/
sudo grub2-mkconfig -o /boot/grub2/grub.cfg  # RHEL
sudo update-grub                               # Debian
```

---

## Ejercicios

### Ejercicio 1: Explorar la configuración del kernel actual

```bash
# Step 1: find and examine your running kernel config
CONFIG=""
if [ -f /boot/config-$(uname -r) ]; then
    CONFIG="/boot/config-$(uname -r)"
elif [ -f /proc/config.gz ]; then
    CONFIG="/proc/config.gz"
fi
echo "Config source: $CONFIG"

# Step 2: count options by type
if [[ "$CONFIG" == *.gz ]]; then
    CFGCMD="zcat $CONFIG"
else
    CFGCMD="cat $CONFIG"
fi

echo "=== Option counts ==="
echo "Built-in (=y):    $($CFGCMD | grep '=y$' | wc -l)"
echo "Module (=m):      $($CFGCMD | grep '=m$' | wc -l)"
echo "Disabled:         $($CFGCMD | grep 'is not set' | wc -l)"

# Step 3: check security options
echo "=== Security ==="
for opt in SECURITY_SELINUX SECURITY_APPARMOR RANDOMIZE_BASE \
    STACKPROTECTOR_STRONG MODULE_SIG LOCK_DOWN_KERNEL; do
    val=$($CFGCMD | grep "CONFIG_${opt}=" | head -1)
    [ -z "$val" ] && val="not set"
    echo "  $opt: $val"
done

# Step 4: check filesystem support
echo "=== Filesystems ==="
for fs in EXT4_FS XFS_FS BTRFS_FS VFAT_FS NFS_FS OVERLAY_FS FUSE_FS; do
    val=$($CFGCMD | grep "CONFIG_${fs}=" | head -1)
    [ -z "$val" ] && val="not set"
    echo "  $fs: $val"
done

# Step 5: check virtualization
echo "=== Virtualization ==="
for opt in KVM KVM_INTEL KVM_AMD VHOST_NET NAMESPACES CGROUPS; do
    val=$($CFGCMD | grep "CONFIG_${opt}=" | head -1)
    [ -z "$val" ] && val="not set"
    echo "  $opt: $val"
done

# Step 6: find the LOCALVERSION
echo "=== Version ==="
$CFGCMD | grep CONFIG_LOCALVERSION
```

**Preguntas:**
1. ¿Cuántas opciones están como built-in vs módulo? ¿Cuál es la proporción?
2. ¿SELinux y AppArmor están habilitados ambos? ¿Es normal?
3. ¿Tu filesystem raíz (ext4/xfs) está como built-in o módulo? ¿Qué implica cada opción?

> **Pregunta de predicción**: si la opción `CONFIG_MODULES=n` (deshabilitar soporte de módulos), ¿qué pasaría con todas las opciones que están como `=m`? ¿Se compilarían como built-in automáticamente o se omitirían?

### Ejercicio 2: Configurar un kernel (sin compilar)

> Este ejercicio solo configura — no compila. Requiere el código fuente descargado.

```bash
# Step 1: install prerequisites
sudo dnf install ncurses-devel make gcc flex bison 2>/dev/null || \
    sudo apt install libncurses-dev make gcc flex bison 2>/dev/null

# Step 2: download kernel source (small, just for config exercise)
cd /tmp
wget -q https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.7.4.tar.xz
tar xf linux-6.7.4.tar.xz
cd linux-6.7.4

# Step 3: create a default config
make defconfig
echo "=== defconfig stats ==="
echo "Built-in: $(grep '=y$' .config | wc -l)"
echo "Module:   $(grep '=m$' .config | wc -l)"

# Step 4: create a localmodconfig (based on loaded modules)
cp .config .config.defconfig
make localmodconfig
echo "=== localmodconfig stats ==="
echo "Built-in: $(grep '=y$' .config | wc -l)"
echo "Module:   $(grep '=m$' .config | wc -l)"

# Step 5: compare sizes
echo "=== Comparison ==="
echo "defconfig modules:      $(grep '=m$' .config.defconfig | wc -l)"
echo "localmodconfig modules: $(grep '=m$' .config | wc -l)"

# Step 6: try menuconfig (explore, don't save)
# make menuconfig
# Navigate: Device Drivers → Network device support → count entries
# Press / and search for EXT4
# Press Esc Esc repeatedly to exit WITHOUT saving

# Step 7: clean up
cd /tmp && rm -rf linux-6.7.4 linux-6.7.4.tar.xz
```

**Preguntas:**
1. ¿Cuántos módulos menos tiene `localmodconfig` vs `defconfig`?
2. ¿Por qué `localmodconfig` es peligroso si planeas usar el kernel en hardware diferente?
3. En `menuconfig`, ¿encontraste la opción EXT4? ¿Bajo qué menú estaba?

> **Pregunta de predicción**: si ejecutas `make localmodconfig` en una máquina donde actualmente no tienes un USB conectado, ¿el kernel resultante soportará USB storage cuando conectes uno después? ¿Por qué?

### Ejercicio 3: Simular el flujo completo (sin instalar)

```bash
# This exercise goes through the ENTIRE compilation flow
# but uses a MINIMAL config to keep compile time short (~5 min)
# We compile but do NOT install (safe)

# Step 1: prepare
cd /tmp
wget -q https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.7.4.tar.xz
tar xf linux-6.7.4.tar.xz
cd linux-6.7.4

# Step 2: create minimal config
make tinyconfig    # Absolute minimum (won't boot, just for testing build)
echo "Tinyconfig options: $(grep '=y' .config | wc -l)"

# Step 3: compile (should be very fast with tinyconfig)
time make -j$(nproc)
# Note the time

# Step 4: check what was produced
ls -lh arch/x86/boot/bzImage    # The kernel image
ls -lh vmlinux                  # Uncompressed kernel
echo "bzImage size: $(du -h arch/x86/boot/bzImage | cut -f1)"
echo "vmlinux size: $(du -h vmlinux | cut -f1)"

# Step 5: count compiled modules
find . -name "*.ko" | wc -l
# Should be 0 or very few with tinyconfig

# Step 6: now try defconfig and compare (optional, takes longer)
# make clean
# make defconfig
# time make -j$(nproc)
# echo "defconfig bzImage: $(du -h arch/x86/boot/bzImage | cut -f1)"
# find . -name "*.ko" | wc -l

# Step 7: clean up
cd /tmp && rm -rf linux-6.7.4 linux-6.7.4.tar.xz
```

**Preguntas:**
1. ¿Cuánto tiempo tardó la compilación con `tinyconfig`? ¿Cuánto esperarías con la config completa de tu distribución?
2. ¿Cuál es el tamaño de `bzImage` con tinyconfig vs el `vmlinuz` de tu distribución en `/boot/`?
3. ¿`tinyconfig` produjo algún módulo `.ko`?

> **Pregunta de predicción**: si compilas con `tinyconfig` e intentas arrancar ese kernel, ¿qué pasará? ¿Llegará siquiera a mostrar mensajes en pantalla? ¿Qué componentes críticos le faltarían?

---

> **Siguiente tema**: T04 — dkms: módulos de terceros, reconstrucción automática al actualizar kernel.
