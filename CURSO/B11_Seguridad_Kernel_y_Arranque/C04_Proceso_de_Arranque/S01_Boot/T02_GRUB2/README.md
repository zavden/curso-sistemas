# GRUB2

## Índice
1. [¿Qué es GRUB2?](#qué-es-grub2)
2. [Arquitectura de GRUB2](#arquitectura-de-grub2)
3. [Configuración: /etc/default/grub](#configuración-etcdefaultgrub)
4. [grub-mkconfig: generar grub.cfg](#grub-mkconfig-generar-grubcfg)
5. [Anatomía de grub.cfg](#anatomía-de-grubcfg)
6. [Parámetros del kernel](#parámetros-del-kernel)
7. [Selección del kernel por defecto](#selección-del-kernel-por-defecto)
8. [Instalación de GRUB2](#instalación-de-grub2)
9. [GRUB2 en RHEL vs Debian](#grub2-en-rhel-vs-debian)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## ¿Qué es GRUB2?

**GRUB2** (GRand Unified Bootloader version 2) es el bootloader estándar de prácticamente todas las distribuciones Linux modernas. Su trabajo es:

1. Presentar un menú de selección de kernel/OS
2. Cargar el kernel (`vmlinuz`) y el disco RAM inicial (`initramfs`) en memoria
3. Pasar parámetros al kernel
4. Transferir el control al kernel

```
  ┌─────────────┐     ┌─────────────┐     ┌──────────────┐
  │  Firmware    │────▶│   GRUB2     │────▶│   Kernel     │
  │  BIOS/UEFI  │     │             │     │   Linux      │
  └─────────────┘     │ 1. Menú     │     └──────────────┘
                      │ 2. Carga    │           │
                      │    vmlinuz  │           ▼
                      │ 3. Carga    │     ┌──────────────┐
                      │    initramfs│     │  initramfs   │
                      │ 4. Pasa     │     │  → mount /   │
                      │    params   │     │  → pivot     │
                      └─────────────┘     │  → /sbin/init│
                                          └──────────────┘
```

GRUB2 reemplazó a GRUB Legacy (0.97) alrededor de 2009. La diferencia clave: GRUB2 genera su configuración automáticamente a partir de scripts, mientras que GRUB Legacy usaba un archivo `menu.lst` editado manualmente.

---

## Arquitectura de GRUB2

### Componentes en disco

```
  ┌──────────────────────────────────────────────────────────────┐
  │                    ARCHIVOS DE GRUB2                          │
  │                                                              │
  │  CONFIGURACIÓN (editable por el admin)                       │
  │  ┌────────────────────────────────────────────────────────┐  │
  │  │ /etc/default/grub          ← Variables principales     │  │
  │  │ /etc/grub.d/               ← Scripts generadores       │  │
  │  │   ├── 00_header            ← Timeout, defaults         │  │
  │  │   ├── 10_linux             ← Entradas de kernels       │  │
  │  │   ├── 20_linux_xen         ← Entradas Xen              │  │
  │  │   ├── 30_os-prober         ← Otros OS (Windows, etc)   │  │
  │  │   ├── 40_custom            ← Entradas manuales         │  │
  │  │   └── 41_custom            ← Más entradas manuales     │  │
  │  └────────────────────────────────────────────────────────┘  │
  │           │                                                  │
  │           │  grub-mkconfig                                   │
  │           ▼                                                  │
  │  GENERADO (NO editar directamente)                           │
  │  ┌────────────────────────────────────────────────────────┐  │
  │  │ /boot/grub2/grub.cfg      ← Config final (RHEL)       │  │
  │  │ /boot/grub/grub.cfg       ← Config final (Debian)     │  │
  │  └────────────────────────────────────────────────────────┘  │
  │                                                              │
  │  MÓDULOS Y DATOS                                             │
  │  ┌────────────────────────────────────────────────────────┐  │
  │  │ /boot/grub2/              ← (RHEL)                    │  │
  │  │ /boot/grub/               ← (Debian)                  │  │
  │  │   ├── i386-pc/            ← Módulos BIOS              │  │
  │  │   ├── x86_64-efi/         ← Módulos UEFI              │  │
  │  │   ├── fonts/              ← Fuentes para el menú      │  │
  │  │   ├── themes/             ← Temas gráficos            │  │
  │  │   ├── grubenv             ← Variables persistentes     │  │
  │  │   └── grub.cfg            ← Config generada           │  │
  │  └────────────────────────────────────────────────────────┘  │
  └──────────────────────────────────────────────────────────────┘
```

### Flujo de generación

El principio fundamental de GRUB2: **nunca editas `grub.cfg` directamente**. En su lugar:

```
  /etc/default/grub          Variables (timeout, default, cmdline)
         +                                    │
  /etc/grub.d/*              Scripts shell     │
         │                                    │
         ▼                                    ▼
  grub2-mkconfig -o /boot/grub2/grub.cfg
         │
         ▼
  /boot/grub2/grub.cfg       Archivo final (auto-generado)
```

Cada script en `/etc/grub.d/` se ejecuta en orden numérico. El resultado combinado se escribe en `grub.cfg`.

---

## Configuración: /etc/default/grub

Este es el archivo principal que editas como administrador. Contiene variables que los scripts de `/etc/grub.d/` leen durante la generación.

### Variables esenciales

```bash
# /etc/default/grub

# Which menu entry to boot by default (number or ID)
GRUB_DEFAULT=0

# Timeout before auto-selecting default (seconds)
GRUB_TIMEOUT=5

# What to show during timeout: "menu", "countdown", "hidden"
GRUB_TIMEOUT_STYLE=menu

# Kernel parameters appended to ALL Linux entries
GRUB_CMDLINE_LINUX="rd.lvm.lv=fedora/root rd.lvm.lv=fedora/swap rhgb quiet"

# Additional params for NORMAL mode only (not recovery)
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash"

# Terminal output mode
GRUB_TERMINAL_OUTPUT="console"

# Disable os-prober (finding other OS like Windows)
GRUB_DISABLE_OS_PROBER=false

# Disable recovery/rescue entries
GRUB_DISABLE_RECOVERY=true

# Disable submenu grouping of old kernels
GRUB_DISABLE_SUBMENU=true
```

### Variables menos comunes pero útiles

```bash
# Serial console (for headless servers)
GRUB_TERMINAL="serial console"
GRUB_SERIAL_COMMAND="serial --speed=115200 --unit=0 --word=8 --parity=no --stop=1"

# Graphics mode for GRUB menu
GRUB_GFXMODE=1024x768

# Pass graphics mode to Linux (graphical boot)
GRUB_GFXPAYLOAD_LINUX=keep

# Custom theme
GRUB_THEME="/boot/grub2/themes/starfield/theme.txt"

# Background image
GRUB_BACKGROUND="/boot/grub2/splash.png"

# Save last selected entry as new default
GRUB_DEFAULT=saved
GRUB_SAVEDEFAULT=true
```

### Desglose de GRUB_CMDLINE_LINUX

Esta variable es la más importante porque define los **parámetros del kernel** para cada arranque:

```bash
GRUB_CMDLINE_LINUX="rd.lvm.lv=fedora/root rd.lvm.lv=fedora/swap rhgb quiet"
#                   ├── rd.lvm.lv=...      ← initramfs: activar LVM
#                   ├── rd.lvm.lv=...      ← initramfs: activar swap LVM
#                   ├── rhgb               ← Red Hat Graphical Boot
#                   └── quiet              ← suprimir mensajes del kernel
```

---

## grub-mkconfig: generar grub.cfg

### Uso básico

```bash
# RHEL/Fedora
sudo grub2-mkconfig -o /boot/grub2/grub.cfg

# Debian/Ubuntu
sudo update-grub
# (wrapper que ejecuta: grub-mkconfig -o /boot/grub/grub.cfg)
```

### Cuándo regenerar grub.cfg

Debes regenerar `grub.cfg` después de:

| Cambio | Ejemplo |
|---|---|
| Editar `/etc/default/grub` | Cambiar timeout, cmdline, default |
| Añadir/eliminar un kernel | `dnf install kernel`, `apt upgrade` |
| Modificar scripts en `/etc/grub.d/` | Agregar entrada custom |
| Cambiar esquema de disco | Nuevo UUID, nueva partición |
| Instalar otro OS | Windows, otra distro |

> **Nota**: muchas distribuciones regeneran `grub.cfg` automáticamente al instalar un nuevo kernel. Pero los cambios en `/etc/default/grub` **siempre** requieren regeneración manual.

### Scripts en /etc/grub.d/

```bash
ls -la /etc/grub.d/
# -rwxr-xr-x  00_header          ← Lee /etc/default/grub, genera encabezado
# -rwxr-xr-x  01_users           ← Contraseña de GRUB (si configurada)
# -rwxr-xr-x  10_linux           ← Detecta kernels en /boot, genera menuentry
# -rwxr-xr-x  20_linux_xen       ← Entradas para Xen
# -rwxr-xr-x  20_ppc_terminfo    ← PowerPC terminal
# -rwxr-xr-x  30_os-prober       ← Detecta otros OS (Windows, etc.)
# -rwxr-xr-x  30_uefi-firmware   ← Entrada para reiniciar al setup UEFI
# -rwxr-xr-x  40_custom          ← TUS entradas manuales (se incluye literal)
# -rwxr-xr-x  41_custom          ← Incluye archivos de otro directorio
```

- El **orden numérico** determina el orden de aparición en `grub.cfg`
- Solo se ejecutan scripts con **permiso de ejecución** (`chmod +x`)
- Para deshabilitar un script: `chmod -x /etc/grub.d/30_os-prober`

### Agregar una entrada personalizada

Edita `/etc/grub.d/40_custom` (no toques los otros scripts):

```bash
#!/bin/sh
exec tail -n +3 $0
# This file provides an easy way to add custom menu entries.

menuentry "Memtest86+" {
    linux16 /boot/memtest86+.bin
}

menuentry "Custom Recovery Shell" {
    set root='hd0,msdos1'
    linux /vmlinuz-5.14.0-362.el9.x86_64 root=/dev/sda2 ro single
    initrd /initramfs-5.14.0-362.el9.x86_64.img
}
```

Después de editar, regenera:

```bash
sudo grub2-mkconfig -o /boot/grub2/grub.cfg
```

---

## Anatomía de grub.cfg

Aunque **nunca debes editar `grub.cfg` directamente**, entender su estructura es esencial para diagnosticar problemas.

### Estructura general

```bash
# grub.cfg (simplified structure)

# ── Generated by 00_header ──
set default="0"
set timeout=5
set timeout_style=menu
load_video
set gfxpayload=keep
insmod gzio
insmod part_gpt
insmod ext2                     # Module to read ext4

# ── Function definitions ──
set root='hd0,gpt2'            # /boot partition
search --no-floppy --fs-uuid --set=root <UUID-of-boot>

# ── Generated by 10_linux ──
menuentry 'Fedora Linux (6.7.4-200.fc39.x86_64)' --class fedora ... {
    load_video
    set gfxpayload=keep
    insmod gzio
    insmod part_gpt
    insmod ext2

    set root='hd0,gpt2'
    search --no-floppy --fs-uuid --set=root <UUID>

    linux   /vmlinuz-6.7.4-200.fc39.x86_64 \
            root=UUID=<root-UUID> ro \
            rd.lvm.lv=fedora/root rhgb quiet

    initrd  /initramfs-6.7.4-200.fc39.x86_64.img
}

menuentry 'Fedora Linux (6.6.9-200.fc39.x86_64)' --class fedora ... {
    # ... same structure, older kernel
}

# ── Generated by 30_os-prober ──
menuentry 'Windows Boot Manager (on /dev/sda1)' --class windows ... {
    insmod part_gpt
    insmod fat
    search --no-floppy --fs-uuid --set=root <ESP-UUID>
    chainloader /EFI/Microsoft/Boot/bootmgfw.efi
}

# ── Generated by 40_custom ──
# Your custom entries here
```

### Elementos clave

| Elemento | Significado |
|---|---|
| `set root='hd0,gpt2'` | Disco 0, partición GPT 2 (donde está /boot) |
| `search --fs-uuid --set=root UUID` | Busca partición por UUID (más robusto que hd0,gpt2) |
| `linux /vmlinuz-...` | Ruta al kernel (relativa a `root`) |
| `initrd /initramfs-...` | Ruta al initramfs |
| `root=UUID=...` | Parámetro kernel: partición raíz del sistema |
| `insmod ext2` | Carga el módulo GRUB para leer ext2/3/4 |
| `chainloader` | Pasa el control a otro bootloader (ej: Windows) |
| `load_video` | Inicializa modo gráfico |

### Diferencia entre root de GRUB y root del kernel

```
  GRUB "set root"                      Kernel "root="
  ┌─────────────────────────┐          ┌─────────────────────────┐
  │ Le dice a GRUB en qué   │          │ Le dice al KERNEL cuál  │
  │ partición buscar los     │          │ es la partición raíz    │
  │ archivos vmlinuz e       │          │ del sistema de archivos │
  │ initramfs.               │          │ (/) que debe montar.    │
  │                          │          │                          │
  │ Ejemplo:                 │          │ Ejemplo:                │
  │ set root='hd0,gpt2'     │          │ root=UUID=abc123        │
  │ = /dev/sda2 = /boot      │          │ = /dev/sda3 = /         │
  └─────────────────────────┘          └─────────────────────────┘
```

Cuando `/boot` es una partición separada, estos dos "root" apuntan a particiones **diferentes**. Cuando `/boot` está dentro de `/`, apuntan a la misma.

---

## Parámetros del kernel

Los parámetros del kernel (también llamados **kernel command line** o **boot parameters**) se pasan desde GRUB al kernel mediante la línea `linux` en `grub.cfg`.

### Cómo se construyen

```
  /etc/default/grub
  GRUB_CMDLINE_LINUX="rd.lvm.lv=fedora/root quiet"
  GRUB_CMDLINE_LINUX_DEFAULT="splash"
         │
         ▼  grub-mkconfig
  grub.cfg:
  linux /vmlinuz-... root=UUID=... ro rd.lvm.lv=fedora/root quiet splash
  │                  │              │  │                     │     │
  │                  │              │  └── GRUB_CMDLINE_LINUX ─────┘
  │                  │              │     (all entries including recovery)
  │                  │              │
  │                  │              └── Read-only root (default)
  │                  └── Root partition (auto-detected)
  └── Kernel path (auto-detected)
```

- `GRUB_CMDLINE_LINUX` → se aplica a **todas** las entradas (normal + recovery)
- `GRUB_CMDLINE_LINUX_DEFAULT` → solo a entradas **normales** (no recovery)

### Parámetros comunes

| Parámetro | Efecto |
|---|---|
| `quiet` | Suprimir la mayoría de mensajes del kernel |
| `splash` | Mostrar pantalla gráfica de arranque (Plymouth) |
| `rhgb` | Red Hat Graphical Boot (equivalente a splash) |
| `ro` | Montar raíz como read-only (fsck la remonta rw) |
| `rw` | Montar raíz como read-write |
| `single` / `1` | Arrancar en modo single-user (rescue) |
| `3` | Arrancar en runlevel 3 (multi-user, sin GUI) |
| `5` | Arrancar en runlevel 5 (graphical) |
| `systemd.unit=rescue.target` | Modo rescue (systemd) |
| `systemd.unit=emergency.target` | Modo emergency (systemd) |
| `init=/bin/bash` | Reemplazar init por una shell (saltarse todo) |
| `rd.break` | Parar en initramfs antes de montar raíz |
| `enforcing=0` | SELinux en modo permissive este arranque |
| `selinux=0` | Deshabilitar SELinux (no recomendado) |
| `audit=1` | Habilitar auditoría del kernel |
| `nomodeset` | No cargar driver de vídeo del kernel (debug GPU) |
| `console=ttyS0,115200` | Redirigir consola a serial |
| `mem=4G` | Limitar RAM visible |
| `crashkernel=auto` | Reservar RAM para kdump |
| `resume=UUID=...` | Partición para reanudar desde hibernación |

### Ver parámetros actuales

```bash
# Current kernel command line
cat /proc/cmdline
# BOOT_IMAGE=(hd0,gpt2)/vmlinuz-6.7.4 root=UUID=... ro rhgb quiet

# All known kernel parameters (documentation)
man bootparam
# Or from kernel source:
# https://www.kernel.org/doc/html/latest/admin-guide/kernel-parameters.html
```

### Parámetros temporales vs permanentes

```
  ┌──────────────────────────────────────────────────────┐
  │              TEMPORAL (este arranque)                  │
  │                                                      │
  │  1. En el menú GRUB, presiona 'e' para editar        │
  │  2. Navega a la línea "linux ..."                    │
  │  3. Añade/modifica parámetros al final               │
  │  4. Ctrl+X o F10 para arrancar                       │
  │                                                      │
  │  → Cambio NO sobrevive al siguiente reinicio          │
  └──────────────────────────────────────────────────────┘

  ┌──────────────────────────────────────────────────────┐
  │              PERMANENTE                               │
  │                                                      │
  │  1. Editar /etc/default/grub                          │
  │     GRUB_CMDLINE_LINUX="... nuevo_param"              │
  │  2. Regenerar:                                       │
  │     sudo grub2-mkconfig -o /boot/grub2/grub.cfg      │
  │  3. Reiniciar                                        │
  │                                                      │
  │  → Cambio persiste en todos los arranques             │
  └──────────────────────────────────────────────────────┘
```

---

## Selección del kernel por defecto

### GRUB_DEFAULT por número

```bash
# Boot the first entry (0-indexed)
GRUB_DEFAULT=0

# Boot the third entry
GRUB_DEFAULT=2
```

El índice depende del orden en que `10_linux` genera las entradas (normalmente kernel más reciente = 0).

### GRUB_DEFAULT por ID

Más robusto que el número, no cambia al instalar nuevos kernels:

```bash
# Find the menuentry IDs
grep -E "^menuentry|^submenu" /boot/grub2/grub.cfg | head -20

# Use the full title
GRUB_DEFAULT="Fedora Linux (6.7.4-200.fc39.x86_64) 39 (Workstation Edition)"

# Or if using submenus, use the path syntax
GRUB_DEFAULT="Advanced options for Fedora>Fedora Linux (6.7.4-200.fc39.x86_64)"
```

### GRUB_DEFAULT=saved

Recuerda el último kernel arrancado o el seleccionado manualmente:

```bash
# In /etc/default/grub
GRUB_DEFAULT=saved
GRUB_SAVEDEFAULT=true

# Regenerate
sudo grub2-mkconfig -o /boot/grub2/grub.cfg

# Manually set the default
sudo grub2-set-default 0                          # By number
sudo grub2-set-default "Fedora Linux (6.7.4-...)" # By title

# Set default for NEXT boot only
sudo grub2-reboot 2    # One-time override

# Check current saved default
sudo grub2-editenv list
# saved_entry=0
```

El valor se guarda en `/boot/grub2/grubenv`:

```bash
cat /boot/grub2/grubenv
# # GRUB Environment Block
# saved_entry=0
# kernelopts=root=UUID=... ro rhgb quiet
# boot_success=1
```

> **Nota**: `grubenv` es un archivo de exactamente 1024 bytes con formato especial. No lo edites manualmente; usa `grub2-editenv`.

### BLS (Boot Loader Specification) — Fedora/RHEL 8+

Fedora y RHEL 8+ usan **BLS (Boot Loader Specification)** en lugar del `10_linux` clásico. Los kernels se definen en archivos individuales:

```bash
ls /boot/loader/entries/
# <machine-id>-6.7.4-200.fc39.x86_64.conf
# <machine-id>-6.6.9-200.fc39.x86_64.conf

cat /boot/loader/entries/*6.7.4*.conf
# title Fedora Linux (6.7.4-200.fc39.x86_64) 39 (Workstation Edition)
# version 6.7.4-200.fc39.x86_64
# linux /vmlinuz-6.7.4-200.fc39.x86_64
# initrd /initramfs-6.7.4-200.fc39.x86_64.img
# options root=UUID=... ro rhgb quiet
# grub_users $grub_users
# grub_arg --unrestricted
# grub_class fedora
```

Con BLS, la variable `kernelopts` en `grubenv` almacena los parámetros del kernel (en lugar de `GRUB_CMDLINE_LINUX`):

```bash
# View current kernel options (BLS systems)
sudo grub2-editenv list | grep kernelopts

# Modify kernel options on BLS systems
sudo grubby --update-kernel=ALL --args="nouveau.modeset=0"
sudo grubby --update-kernel=ALL --remove-args="rhgb quiet"

# Set default kernel with grubby
sudo grubby --set-default /boot/vmlinuz-6.7.4-200.fc39.x86_64
sudo grubby --default-kernel    # Show current default
sudo grubby --default-index     # Show current default index
```

---

## Instalación de GRUB2

### En BIOS (MBR)

```bash
# Install GRUB to MBR of disk (NOT partition)
sudo grub2-install /dev/sda          # RHEL
sudo grub-install /dev/sda           # Debian

# What it does:
# 1. Writes boot.img to MBR sector 0 (first 446 bytes)
# 2. Writes core.img to post-MBR sectors (1-62)
# 3. core.img contains: diskboot.img + modules (ext2, biosdisk, etc)
```

### En UEFI

```bash
# Install GRUB EFI binary to ESP
sudo grub2-install --target=x86_64-efi \
    --efi-directory=/boot/efi \
    --bootloader-id=fedora

# What it does:
# 1. Copies grubx64.efi to /boot/efi/EFI/fedora/
# 2. Installs modules to /boot/efi/EFI/fedora/
# 3. Creates NVRAM boot entry via efibootmgr

# Install to removable media path (USB, no NVRAM entry)
sudo grub2-install --target=x86_64-efi \
    --efi-directory=/boot/efi \
    --removable
# Installs to /boot/efi/EFI/BOOT/BOOTX64.EFI
```

### Reinstalar GRUB desde un live USB (rescue)

Cuando GRUB está roto y el sistema no arranca:

```bash
# Boot from live USB, then:

# 1. Find your root partition
lsblk -f

# 2. Mount the root filesystem
sudo mount /dev/sda3 /mnt          # or your root partition
sudo mount /dev/sda2 /mnt/boot     # if /boot is separate
sudo mount /dev/sda1 /mnt/boot/efi # ESP (UEFI only)

# 3. Mount pseudo-filesystems
sudo mount --bind /dev  /mnt/dev
sudo mount --bind /proc /mnt/proc
sudo mount --bind /sys  /mnt/sys
sudo mount --bind /run  /mnt/run

# 4. Chroot into the system
sudo chroot /mnt

# 5. Reinstall GRUB
# BIOS:
grub2-install /dev/sda
# UEFI:
grub2-install --target=x86_64-efi --efi-directory=/boot/efi

# 6. Regenerate config
grub2-mkconfig -o /boot/grub2/grub.cfg

# 7. Exit and reboot
exit
sudo umount -R /mnt
sudo reboot
```

---

## GRUB2 en RHEL vs Debian

| Aspecto | RHEL/Fedora | Debian/Ubuntu |
|---|---|---|
| **Comando** | `grub2-mkconfig` | `grub-mkconfig` |
| **Wrapper** | – | `update-grub` |
| **Config path** | `/boot/grub2/grub.cfg` | `/boot/grub/grub.cfg` |
| **Módulos** | `/boot/grub2/i386-pc/` | `/boot/grub/i386-pc/` |
| **Install** | `grub2-install` | `grub-install` |
| **Set default** | `grub2-set-default` | `grub-set-default` |
| **Reboot once** | `grub2-reboot` | `grub-reboot` |
| **Edit env** | `grub2-editenv` | `grub-editenv` |
| **BLS** | Sí (RHEL 8+, Fedora 30+) | No |
| **grubby** | Sí | No (usa `update-grub`) |
| **Paquete** | `grub2-tools`, `grub2-efi-x64` | `grub2-common`, `grub-efi-amd64` |

> **Tip**: en RHEL, la mayoría de comandos son symlinks: `grub2-mkconfig` → `grub-mkconfig`. Pero siempre usa el nombre con el prefijo `grub2-` en RHEL por claridad.

---

## Errores comunes

### 1. Editar grub.cfg directamente

El archivo se sobrescribe cada vez que ejecutas `grub-mkconfig` o instalas un kernel nuevo. Tus cambios se pierden silenciosamente:

```bash
# WRONG:
sudo vim /boot/grub2/grub.cfg   # Will be overwritten!

# RIGHT:
sudo vim /etc/default/grub
sudo grub2-mkconfig -o /boot/grub2/grub.cfg
```

### 2. Olvidar regenerar grub.cfg después de editar /etc/default/grub

Editar `/etc/default/grub` no tiene efecto hasta que regeneras:

```bash
# After any change to /etc/default/grub:
sudo grub2-mkconfig -o /boot/grub2/grub.cfg   # RHEL
sudo update-grub                                # Debian
```

### 3. Usar grub2-install en la partición en vez del disco

```bash
# WRONG — installs to partition, breaks boot
sudo grub2-install /dev/sda1

# RIGHT — installs to disk MBR
sudo grub2-install /dev/sda
```

`grub2-install` escribe el bootstrap code en el MBR del **disco**, no en el boot sector de una partición.

### 4. Confundir GRUB_DEFAULT con submenús

Si GRUB agrupa kernels antiguos en un submenú "Advanced options", el índice `0` es el primer menuentry, `1` es el submenú, y para seleccionar un kernel dentro del submenú necesitas la sintaxis `"submenu_title>entry_title"`:

```bash
# With submenus:
# 0 = "Fedora (6.7.4)"
# 1 = "Advanced options" (submenu)
#     1>0 = "Fedora (6.6.9)"
#     1>1 = "Fedora (6.6.9) recovery"

GRUB_DEFAULT="1>0"   # Second kernel inside submenu
```

### 5. No entender la diferencia GRUB_CMDLINE_LINUX vs _DEFAULT

```bash
# This goes to ALL entries (including recovery):
GRUB_CMDLINE_LINUX="rd.lvm.lv=fedora/root"

# This goes ONLY to normal entries (NOT recovery):
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash"

# Recovery mode gets: rd.lvm.lv=fedora/root (but NOT quiet splash)
# Normal mode gets:   rd.lvm.lv=fedora/root quiet splash
```

Si pones parámetros esenciales (como `root=` o `rd.lvm.lv=`) en `_DEFAULT`, el modo recovery no los tendrá y fallará.

---

## Cheatsheet

```bash
# ═══════════════════════════════════════════════
#  CONFIGURACIÓN
# ═══════════════════════════════════════════════
sudo vim /etc/default/grub              # Edit main config
sudo grub2-mkconfig -o /boot/grub2/grub.cfg  # RHEL: regenerate
sudo update-grub                              # Debian: regenerate

# ═══════════════════════════════════════════════
#  SELECCIÓN DE KERNEL
# ═══════════════════════════════════════════════
sudo grub2-set-default 0                # Set default by index
sudo grub2-set-default "Title..."       # Set default by title
sudo grub2-reboot 2                     # One-time next boot
sudo grub2-editenv list                 # Show saved entry
cat /proc/cmdline                       # Current kernel params

# ═══════════════════════════════════════════════
#  BLS (RHEL 8+ / FEDORA 30+)
# ═══════════════════════════════════════════════
ls /boot/loader/entries/                # List BLS entries
sudo grubby --default-kernel            # Show default kernel
sudo grubby --set-default /boot/vmlinuz-...   # Set default
sudo grubby --update-kernel=ALL --args="param=value"  # Add param
sudo grubby --update-kernel=ALL --remove-args="param" # Remove param
sudo grubby --info=ALL                  # Show all entries

# ═══════════════════════════════════════════════
#  INSTALACIÓN
# ═══════════════════════════════════════════════
# BIOS:
sudo grub2-install /dev/sda
# UEFI:
sudo grub2-install --target=x86_64-efi \
    --efi-directory=/boot/efi --bootloader-id=fedora

# ═══════════════════════════════════════════════
#  DIAGNÓSTICO
# ═══════════════════════════════════════════════
cat /proc/cmdline                       # Active kernel params
grep menuentry /boot/grub2/grub.cfg     # List menu entries
sudo grub2-editenv list                 # grubenv variables
ls /boot/vmlinuz-*                      # Available kernels
ls /boot/initramfs-*                    # Available initramfs

# ═══════════════════════════════════════════════
#  ENTRADAS PERSONALIZADAS
# ═══════════════════════════════════════════════
sudo vim /etc/grub.d/40_custom          # Add custom entries
sudo grub2-mkconfig -o /boot/grub2/grub.cfg  # Regenerate
```

---

## Ejercicios

### Ejercicio 1: Inspeccionar tu configuración de GRUB2

```bash
# Step 1: view current GRUB defaults
cat /etc/default/grub

# Step 2: identify which kernel boots by default
sudo grub2-editenv list 2>/dev/null      # BLS systems
grep GRUB_DEFAULT /etc/default/grub      # Classic systems

# Step 3: list available kernels
ls -lt /boot/vmlinuz-*

# Step 4: view current kernel params
cat /proc/cmdline

# Step 5: list menuentry titles in grub.cfg
grep -E "^menuentry|^submenu" /boot/grub2/grub.cfg 2>/dev/null || \
grep -E "^menuentry|^submenu" /boot/grub/grub.cfg

# Step 6: check for BLS entries (Fedora/RHEL 8+)
ls /boot/loader/entries/ 2>/dev/null && \
    cat /boot/loader/entries/*.conf | head -30

# Step 7: compare GRUB_CMDLINE_LINUX with /proc/cmdline
grep GRUB_CMDLINE /etc/default/grub
cat /proc/cmdline
```

**Preguntas:**
1. ¿Cuántos kernels tienes instalados? ¿Cuál es el default?
2. ¿Hay parámetros en `/proc/cmdline` que no están en `GRUB_CMDLINE_LINUX`? ¿De dónde vienen?
3. ¿Tu sistema usa BLS o el método clásico?

> **Pregunta de predicción**: si cambias `GRUB_TIMEOUT=5` a `GRUB_TIMEOUT=0` en `/etc/default/grub` pero no ejecutas `grub-mkconfig`, ¿el próximo arranque mostrará el menú 5 segundos o 0 segundos? ¿Por qué?

### Ejercicio 2: Modificar parámetros del kernel

```bash
# Step 1: view current params
cat /proc/cmdline

# Step 2: temporarily remove "quiet" for verbose boot
#   → In GRUB menu, press 'e'
#   → Find "linux" line, delete "quiet" and "rhgb"/"splash"
#   → Ctrl+X to boot
#   → Watch the verbose kernel messages

# Step 3: make it permanent (add a diagnostic parameter)
# Add net.ifnames=0 to disable predictable network names (as exercise only)
sudo cp /etc/default/grub /etc/default/grub.bak

# View current GRUB_CMDLINE_LINUX
grep GRUB_CMDLINE_LINUX /etc/default/grub

# Add a harmless parameter (audit=1 to enable kernel auditing)
# Edit /etc/default/grub and add audit=1 to GRUB_CMDLINE_LINUX
sudo vim /etc/default/grub

# Step 4: regenerate and verify (DON'T reboot yet)
sudo grub2-mkconfig -o /boot/grub2/grub.cfg 2>/dev/null || \
    sudo update-grub

# Step 5: verify the parameter appears in grub.cfg
grep "audit=1" /boot/grub2/grub.cfg 2>/dev/null || \
    grep "audit=1" /boot/grub/grub.cfg

# Step 6: restore backup
sudo cp /etc/default/grub.bak /etc/default/grub
sudo grub2-mkconfig -o /boot/grub2/grub.cfg 2>/dev/null || \
    sudo update-grub
```

**Preguntas:**
1. Al quitar `quiet` en el paso 2, ¿cuántos mensajes adicionales aparecieron durante el arranque?
2. ¿El parámetro `audit=1` apareció en **todas** las entradas de `grub.cfg` o solo en las normales?
3. Si hubieras puesto `audit=1` en `GRUB_CMDLINE_LINUX_DEFAULT` en vez de `GRUB_CMDLINE_LINUX`, ¿en qué entradas aparecería?

> **Pregunta de predicción**: si añades `systemd.unit=multi-user.target` a `GRUB_CMDLINE_LINUX`, ¿qué efecto tendrá en el arranque? ¿Afectará también al modo recovery?

### Ejercicio 3: Gestionar el kernel por defecto

```bash
# Step 1: list available kernels
ls -1 /boot/vmlinuz-* | sort -V

# Step 2: check current default
# BLS:
sudo grubby --default-kernel 2>/dev/null
# Classic:
sudo grub2-editenv list 2>/dev/null

# Step 3: check GRUB_DEFAULT value
grep GRUB_DEFAULT /etc/default/grub

# Step 4 (BLS systems): set an older kernel as default
OLDER_KERNEL=$(ls /boot/vmlinuz-* | sort -V | head -1)
echo "Will set default to: $OLDER_KERNEL"
# sudo grubby --set-default $OLDER_KERNEL   # Uncomment to execute

# Step 5 (Classic systems): use grub2-set-default
# sudo grub2-set-default 1   # Second entry

# Step 6: verify
sudo grubby --default-kernel 2>/dev/null || sudo grub2-editenv list

# Step 7: set back to newest kernel
NEWEST_KERNEL=$(ls /boot/vmlinuz-* | sort -V | tail -1)
# sudo grubby --set-default $NEWEST_KERNEL   # Uncomment to execute

# Step 8: test one-time boot (without changing default)
# sudo grub2-reboot 1
# This would boot entry 1 ONCE, then return to default
```

**Preguntas:**
1. ¿Cuál es la diferencia práctica entre `grub2-set-default` y `grub2-reboot`?
2. Si usas `GRUB_DEFAULT=saved` con `GRUB_SAVEDEFAULT=true`, ¿qué pasa si seleccionas manualmente un kernel diferente en el menú?
3. ¿Dónde almacena GRUB la selección "saved"?

> **Pregunta de predicción**: tienes 3 kernels instalados y `GRUB_DEFAULT=0`. Instalas un kernel nuevo. ¿El nuevo kernel se convierte automáticamente en el default? ¿Qué pasa si `GRUB_DEFAULT=saved` y el kernel guardado era el anterior más reciente?

---

> **Siguiente tema**: T03 — Edición de GRUB en boot: recovery mode, init=/bin/bash, password de GRUB.
