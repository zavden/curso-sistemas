# Proceso de Arranque Completo

## Índice
1. [Visión general](#visión-general)
2. [Fase 1: Firmware (BIOS/UEFI)](#fase-1-firmware-biosuefi)
3. [Fase 2: Bootloader (GRUB2)](#fase-2-bootloader-grub2)
4. [Fase 3: Kernel](#fase-3-kernel)
5. [Fase 4: initramfs](#fase-4-initramfs)
6. [Fase 5: init (systemd)](#fase-5-init-systemd)
7. [Fase 6: Target (runlevel)](#fase-6-target-runlevel)
8. [Línea temporal completa](#línea-temporal-completa)
9. [Diagnóstico del arranque](#diagnóstico-del-arranque)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## Visión general

El arranque de Linux es una cadena de 6 fases donde cada componente inicializa lo necesario y transfiere el control al siguiente:

```
  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐
  │ Firmware │──▶│Bootloader│──▶│  Kernel  │──▶│initramfs │──▶│  init    │──▶│  Target  │
  │BIOS/UEFI│   │  GRUB2   │   │  vmlinuz │   │ (rootfs  │   │ systemd  │   │graphical/│
  │          │   │          │   │          │   │ temporal)│   │ PID 1    │   │multi-user│
  └──────────┘   └──────────┘   └──────────┘   └──────────┘   └──────────┘   └──────────┘
   Hardware       Disco/ESP      /boot          /boot          /sbin/init     Servicios
   POST           Menú           Descomprime    Monta root     Units          Login
   Busca boot     Carga kernel   Detecta HW     pivot_root     Dependencias   Sesión
```

Cada fase resuelve un problema específico:

| Fase | Problema que resuelve |
|---|---|
| **Firmware** | "¿Qué hardware tengo y dónde está el bootloader?" |
| **Bootloader** | "¿Qué kernel cargo y con qué parámetros?" |
| **Kernel** | "¿Cómo inicializo el hardware y preparo el entorno?" |
| **initramfs** | "¿Cómo accedo al root filesystem si necesito drivers especiales?" |
| **init** | "¿Qué servicios arranco y en qué orden?" |
| **Target** | "¿Qué entorno final presento al usuario?" |

---

## Fase 1: Firmware (BIOS/UEFI)

### BIOS

```
  Encendido (PSU → Power Good signal)
       │
       ▼
  CPU ejecuta instrucción en 0xFFFFFFF0
  (reset vector → apunta al ROM del BIOS)
       │
       ▼
  ┌─────────────────────────────────────────┐
  │              P O S T                     │
  │  Power-On Self-Test                      │
  │                                          │
  │  1. Test del CPU (registros, flags)      │
  │  2. Verificar checksum del BIOS ROM      │
  │  3. Test de RAM (contar y verificar)     │
  │  4. Inicializar chipset (puente norte/   │
  │     sur, PCH moderno)                    │
  │  5. Enumerar buses: PCI, USB, SATA      │
  │  6. Inicializar vídeo (BIOS de la GPU)  │
  │  7. Detectar dispositivos de arranque    │
  └─────────────────┬───────────────────────┘
                    │
                    ▼
  Leer sector 0 (MBR, 512 bytes) del primer
  dispositivo de arranque
       │
       ▼
  Verificar firma 0x55AA
       │
       ▼
  Ejecutar bootstrap code (446 bytes)
  → Control pasa al bootloader
```

### UEFI

```
  Encendido
       │
       ▼
  ┌──────────────────────────────────┐
  │  SEC (Security Phase)            │
  │  Inicialización mínima del CPU   │
  │  Verificar integridad del FW     │
  └──────────────┬───────────────────┘
                 │
                 ▼
  ┌──────────────────────────────────┐
  │  PEI (Pre-EFI Initialization)   │
  │  Detectar y configurar RAM      │
  │  Crear HOB (Hand-Off Blocks)    │
  └──────────────┬───────────────────┘
                 │
                 ▼
  ┌──────────────────────────────────┐
  │  DXE (Driver Execution Env)     │
  │  Cargar drivers UEFI            │
  │  (disco, USB, red, gráficos)    │
  │  Enumerar dispositivos          │
  │  Instalar protocolos            │
  └──────────────┬───────────────────┘
                 │
                 ▼
  ┌──────────────────────────────────┐
  │  BDS (Boot Device Selection)    │
  │  Leer variables Boot#### NVRAM  │
  │  Consultar BootOrder            │
  │  Localizar archivo .efi en ESP  │
  └──────────────┬───────────────────┘
                 │
                 ▼
  ┌──────────────────────────────────┐
  │  Secure Boot (si habilitado)    │
  │  Verificar firma de .efi        │
  │  contra db/dbx                  │
  └──────────────┬───────────────────┘
                 │
                 ▼
  Ejecutar shimx64.efi → grubx64.efi
  → Control pasa al bootloader
```

**Duración típica**: 1-5 segundos (UEFI) a 5-30 segundos (BIOS antiguo).

---

## Fase 2: Bootloader (GRUB2)

GRUB2 recibe el control del firmware. Su trabajo es cargar el kernel y el initramfs en RAM.

```
  ┌─────────────────────────────────────────────────────────────┐
  │                         GRUB2                                │
  │                                                             │
  │  1. Cargar módulos necesarios                               │
  │     insmod part_gpt    (leer tabla GPT)                     │
  │     insmod ext2        (leer ext4)                          │
  │     insmod lvm         (si /boot está en LVM)               │
  │                                                             │
  │  2. Localizar /boot                                         │
  │     set root='hd0,gpt2'                                    │
  │     search --fs-uuid --set=root <UUID>                      │
  │                                                             │
  │  3. Mostrar menú (si GRUB_TIMEOUT > 0)                     │
  │     ┌──────────────────────────────────┐                    │
  │     │ Fedora (6.7.4)            ◄──   │                    │
  │     │ Fedora (6.6.9)                  │                    │
  │     │ Windows Boot Manager            │                    │
  │     └──────────────────────────────────┘                    │
  │                                                             │
  │  4. Cargar kernel en RAM                                    │
  │     linux /vmlinuz-6.7.4 root=UUID=... ro quiet             │
  │     ┌────────────────────────────────────────┐              │
  │     │ vmlinuz (~10 MB comprimido)            │              │
  │     │ Se carga en una dirección de RAM       │              │
  │     │ específica (tipicamente > 1 MB)        │              │
  │     └────────────────────────────────────────┘              │
  │                                                             │
  │  5. Cargar initramfs en RAM                                 │
  │     initrd /initramfs-6.7.4.img                             │
  │     ┌────────────────────────────────────────┐              │
  │     │ initramfs (~30-80 MB comprimido)       │              │
  │     │ Se carga contiguo al kernel            │              │
  │     └────────────────────────────────────────┘              │
  │                                                             │
  │  6. Pasar parámetros al kernel (command line)               │
  │     root=UUID=... ro quiet rhgb                             │
  │                                                             │
  │  7. Transferir control al kernel                            │
  │     → GRUB deja de existir                                  │
  └─────────────────────────────────────────────────────────────┘
```

> **Punto clave**: después de que GRUB transfiere el control, **desaparece completamente** de la memoria. El kernel no sabe ni le importa qué bootloader lo cargó.

---

## Fase 3: Kernel

El kernel recibe el control comprimido en RAM. Debe descomprimirse, inicializar todo el hardware y prepararse para montar el root filesystem.

```
  ┌─────────────────────────────────────────────────────────────────┐
  │                     KERNEL BOOT                                  │
  │                                                                 │
  │  ┌───────────────────────────────────────────────────────────┐  │
  │  │ ETAPA 1: Descompresión                                    │  │
  │  │                                                           │  │
  │  │ vmlinuz contiene:                                         │  │
  │  │ ┌──────────┐ ┌────────────────────────────────────────┐   │  │
  │  │ │ Header + │ │ Kernel comprimido (gzip/xz/zstd/lz4)  │   │  │
  │  │ │ decom-   │ │                                        │   │  │
  │  │ │ pressor  │ │ ~10 MB comprimido → ~30-50 MB en RAM   │   │  │
  │  │ └──────────┘ └────────────────────────────────────────┘   │  │
  │  │                                                           │  │
  │  │ "Decompressing Linux... done, booting the kernel."        │  │
  │  └───────────────────────────────────────────────────────────┘  │
  │                          │                                      │
  │                          ▼                                      │
  │  ┌───────────────────────────────────────────────────────────┐  │
  │  │ ETAPA 2: Inicialización temprana (start_kernel)           │  │
  │  │                                                           │  │
  │  │ • Configurar tablas de páginas (memoria virtual)          │  │
  │  │ • Configurar IDT/GDT (interrupciones, segmentos)         │  │
  │  │ • Inicializar consola temprana (earlycon)                 │  │
  │  │ • Parsear kernel command line (de GRUB)                   │  │
  │  │ • Detectar CPU (vendor, features, bugs/mitigations)       │  │
  │  │ • Inicializar memory allocator (SLUB/SLAB)                │  │
  │  │ • Configurar interrupciones y timers                      │  │
  │  │ • Inicializar scheduler                                   │  │
  │  └───────────────────────────────────────────────────────────┘  │
  │                          │                                      │
  │                          ▼                                      │
  │  ┌───────────────────────────────────────────────────────────┐  │
  │  │ ETAPA 3: Inicialización de subsistemas                   │  │
  │  │                                                           │  │
  │  │ • Bus PCI: enumerar todos los dispositivos                │  │
  │  │ • ACPI: tablas de configuración del firmware              │  │
  │  │ • Drivers compilados en el kernel (built-in, no modules)  │  │
  │  │   - Driver de disco (ahci, nvme, virtio-blk)              │  │
  │  │   - Driver de consola (fbcon, efifb)                      │  │
  │  │ • Networking stack (pero sin configurar interfaces)       │  │
  │  │ • Inicializar /dev, /proc, /sys (internos del kernel)     │  │
  │  └───────────────────────────────────────────────────────────┘  │
  │                          │                                      │
  │                          ▼                                      │
  │  ┌───────────────────────────────────────────────────────────┐  │
  │  │ ETAPA 4: Montar initramfs                                │  │
  │  │                                                           │  │
  │  │ • Crear tmpfs en /                                        │  │
  │  │ • Descomprimir initramfs (cpio archive) en tmpfs          │  │
  │  │ • Ejecutar /init del initramfs                            │  │
  │  │   → Control pasa al initramfs                             │  │
  │  └───────────────────────────────────────────────────────────┘  │
  └─────────────────────────────────────────────────────────────────┘
```

### ¿Qué está compilado en el kernel vs qué es módulo?

```
  ┌───────────────────────────────────────────────────────────┐
  │                    vmlinuz (built-in)                      │
  │                                                           │
  │  Todo lo que el kernel necesita ANTES de tener acceso     │
  │  al root filesystem:                                      │
  │                                                           │
  │  • CPU scheduler, memory management                       │
  │  • VFS (Virtual Filesystem Switch)                        │
  │  • Console drivers (para mostrar mensajes)                │
  │  • Soporte de initramfs (ramfs/tmpfs)                     │
  │  • Opcionalmente: drivers de disco básicos                │
  │                                                           │
  │  Config: =y en .config (compiled-in)                      │
  └───────────────────────────────────────────────────────────┘

  ┌───────────────────────────────────────────────────────────┐
  │           initramfs (módulos .ko cargables)               │
  │                                                           │
  │  Drivers que el kernel necesita para acceder al disco     │
  │  real pero que no están built-in:                         │
  │                                                           │
  │  • Storage: ahci, nvme, virtio_blk, sd_mod               │
  │  • Filesystem: ext4, xfs, btrfs                           │
  │  • Device Mapper: dm-mod (LVM), dm-crypt (LUKS)          │
  │  • RAID: md-mod, raid456                                   │
  │  • Network: drivers para NFS root o iSCSI                 │
  │                                                           │
  │  Config: =m en .config (module, cargado por initramfs)    │
  └───────────────────────────────────────────────────────────┘

  ┌───────────────────────────────────────────────────────────┐
  │         /lib/modules/$(uname -r)/ (post-boot)            │
  │                                                           │
  │  Todo lo demás: drivers de red, sonido, USB, GPU,        │
  │  filesystems adicionales, etc.                            │
  │  Se cargan bajo demanda después del arranque.             │
  └───────────────────────────────────────────────────────────┘
```

---

## Fase 4: initramfs

El **initramfs** (initial RAM filesystem) es un filesystem temporal que contiene las herramientas y módulos necesarios para montar el root filesystem real. Existe porque el kernel necesita drivers (que normalmente son módulos) para acceder al disco, pero los módulos están **en el disco**.

### El problema del huevo y la gallina

```
  ┌──────────────────────────────────────────────────────────┐
  │                  EL PROBLEMA                              │
  │                                                          │
  │  El kernel necesita            Para acceder al disco     │
  │  cargar módulos         ───▶   necesita módulos          │
  │  (ej: ext4.ko, nvme.ko)       que están en el disco      │
  │                                                          │
  │           ┌──────────┐                                   │
  │           │  Módulos │ ◄─── Están aquí                   │
  │           │ en disco │                                   │
  │           │ /lib/    │ ◄─── Pero para leerlos            │
  │           │ modules/ │      necesito el driver            │
  │           └──────────┘      del disco...                  │
  │                                                          │
  │                  LA SOLUCIÓN                              │
  │                                                          │
  │  initramfs: un filesystem temporal en RAM que            │
  │  contiene SOLO los módulos necesarios para               │
  │  acceder al disco real.                                  │
  │                                                          │
  │  ┌────────────┐    ┌──────────┐    ┌──────────────┐      │
  │  │ initramfs  │───▶│ Cargar   │───▶│ Montar root  │      │
  │  │ en RAM     │    │ nvme.ko  │    │ real desde   │      │
  │  │ (de GRUB)  │    │ ext4.ko  │    │ disco        │      │
  │  └────────────┘    └──────────┘    └──────────────┘      │
  └──────────────────────────────────────────────────────────┘
```

### Contenido del initramfs

```bash
# Examine initramfs contents (without extracting)
lsinitrd /boot/initramfs-$(uname -r).img          # RHEL/Fedora
lsinitramfs /boot/initrd.img-$(uname -r)          # Debian/Ubuntu

# Extract initramfs to inspect
mkdir /tmp/initramfs && cd /tmp/initramfs
# RHEL (dracut):
/usr/lib/dracut/skipcpio /boot/initramfs-$(uname -r).img | \
    zcat | cpio -idmv
# Debian:
unmkinitramfs /boot/initrd.img-$(uname -r) /tmp/initramfs
```

Estructura típica:

```
  initramfs (en RAM)
  /
  ├── bin/          ← Binarios esenciales (busybox o systemd)
  ├── sbin/         ← cryptsetup, lvm, fsck, modprobe
  ├── lib/
  │   └── modules/  ← SOLO los módulos necesarios para el boot
  │       └── 6.7.4-200.fc39.x86_64/
  │           ├── kernel/
  │           │   ├── drivers/ata/       (ahci)
  │           │   ├── drivers/nvme/      (nvme)
  │           │   ├── drivers/scsi/      (sd_mod)
  │           │   ├── drivers/md/        (dm-mod, dm-crypt)
  │           │   └── fs/ext4/           (ext4)
  │           └── modules.dep
  ├── etc/
  │   ├── modprobe.d/
  │   └── crypttab        ← Si hay LUKS
  ├── usr/
  │   ├── bin/
  │   └── lib/systemd/    ← systemd dentro del initramfs (dracut)
  └── init               ← Script/binario principal (PID 1 temporal)
```

### Flujo del initramfs

```
  Kernel ejecuta /init del initramfs
       │
       ▼
  ┌─────────────────────────────────────────────┐
  │ 1. Configurar entorno mínimo                │
  │    • Montar /proc, /sys, /dev (devtmpfs)    │
  │    • Iniciar udev (detectar hardware)        │
  └──────────────────┬──────────────────────────┘
                     │
                     ▼
  ┌─────────────────────────────────────────────┐
  │ 2. Cargar módulos de almacenamiento         │
  │    • modprobe ahci / nvme / virtio_blk      │
  │    • modprobe sd_mod (SCSI disk)            │
  │    → Los discos aparecen como /dev/sd*      │
  └──────────────────┬──────────────────────────┘
                     │
                     ▼
  ┌─────────────────────────────────────────────┐
  │ 3. Ensamblar almacenamiento complejo        │
  │    • RAID: mdadm --assemble                 │
  │    • LVM: vgchange -ay                      │
  │    • LUKS: cryptsetup luksOpen              │
  │      (pide passphrase si no hay keyfile)    │
  └──────────────────┬──────────────────────────┘
                     │
                     ▼
  ┌─────────────────────────────────────────────┐
  │ 4. Encontrar root filesystem                │
  │    • Parsear root=UUID=... del cmdline      │
  │    • Esperar a que el dispositivo aparezca   │
  │      (rootwait, rootdelay)                  │
  └──────────────────┬──────────────────────────┘
                     │
                     ▼
  ┌─────────────────────────────────────────────┐
  │ 5. Verificar filesystem (fsck)              │
  │    • Solo si se montará ro y necesita check  │
  └──────────────────┬──────────────────────────┘
                     │
                     ▼
  ┌─────────────────────────────────────────────┐
  │ 6. Montar root en /sysroot (read-only)      │
  │    • mount -o ro /dev/mapper/fedora-root     │
  │      /sysroot                               │
  └──────────────────┬──────────────────────────┘
                     │
                     ▼
  ┌─────────────────────────────────────────────┐
  │ 7. pivot_root (o switch_root)               │
  │    • /sysroot se convierte en /             │
  │    • initramfs se libera de la RAM          │
  │    • exec /sbin/init (systemd del real /)   │
  │    → Control pasa a systemd                 │
  └─────────────────────────────────────────────┘
```

### rd.break: dónde para

```
  Paso 6 (mount /sysroot)
       │
       ├── rd.break → PARA AQUÍ (antes de pivot_root)
       │               /sysroot montado ro
       │               Tú haces chroot /sysroot
       │
       ▼
  Paso 7 (pivot_root → exec /sbin/init)
       │
       └── init=/bin/bash → reemplaza /sbin/init por bash
```

---

## Fase 5: init (systemd)

Después del `pivot_root`, el kernel ejecuta `/sbin/init` del root filesystem real. En distribuciones modernas, `/sbin/init` es un symlink a **systemd**.

```bash
ls -la /sbin/init
# lrwxrwxrwx 1 root root 22 ... /sbin/init -> ../lib/systemd/systemd
```

### PID 1: systemd

```
  ┌─────────────────────────────────────────────────────────────┐
  │                     systemd (PID 1)                          │
  │                                                             │
  │  1. Leer configuración                                      │
  │     • /etc/systemd/system.conf                              │
  │     • Determinar default.target                             │
  │       (symlink en /etc/systemd/system/)                     │
  │                                                             │
  │  2. Construir árbol de dependencias                         │
  │     • default.target                                        │
  │       └── Requires: multi-user.target                       │
  │             ├── Requires: basic.target                      │
  │             │     ├── Requires: sysinit.target              │
  │             │     │     ├── Requires: local-fs.target       │
  │             │     │     ├── Requires: swap.target           │
  │             │     │     └── systemd-tmpfiles-setup          │
  │             │     ├── Requires: sockets.target              │
  │             │     └── Requires: timers.target               │
  │             ├── sshd.service                                │
  │             ├── NetworkManager.service                      │
  │             ├── crond.service                               │
  │             └── ...                                         │
  │                                                             │
  │  3. Arrancar units respetando dependencias                  │
  │     • Paralelización: units sin dependencias                │
  │       mutuas arrancan en paralelo                           │
  │     • After/Before: ordenamiento temporal                   │
  │     • Requires/Wants: dependencias obligatorias/opcionales  │
  │                                                             │
  │  4. Alcanzar el target configurado                          │
  │     • graphical.target: display manager + escritorio        │
  │     • multi-user.target: login en consola                   │
  └─────────────────────────────────────────────────────────────┘
```

### Orden de los targets

```
  sysinit.target        Inicialización del sistema
       │                  • Montar filesystems (/etc/fstab)
       │                  • Activar swap
       │                  • Configurar hostname
       │                  • Cargar módulos del kernel
       │                  • Configurar SELinux/AppArmor
       ▼
  basic.target          Servicios básicos
       │                  • Sockets de systemd
       │                  • Timers
       │                  • Paths
       │                  • Slices (cgroups)
       ▼
  multi-user.target     Sistema multi-usuario
       │                  • sshd, crond, NetworkManager
       │                  • rsyslog/journald
       │                  • Servicios de red
       │                  • Login en consola
       ▼
  graphical.target      Entorno gráfico
                          • Display manager (GDM, SDDM, LightDM)
                          • Escritorio (GNOME, KDE, etc.)
```

### Targets equivalentes a runlevels

| Runlevel (SysVinit) | Target (systemd) | Descripción |
|---|---|---|
| 0 | `poweroff.target` | Apagar |
| 1 | `rescue.target` | Single-user, red deshabilitada |
| 2 | `multi-user.target` | Multi-user sin NFS (legacy) |
| 3 | `multi-user.target` | Multi-user con red, sin GUI |
| 4 | `multi-user.target` | Personalizado (no usado) |
| 5 | `graphical.target` | Multi-user con GUI |
| 6 | `reboot.target` | Reiniciar |

```bash
# View default target
systemctl get-default
# graphical.target

# Change default target
sudo systemctl set-default multi-user.target

# Switch target at runtime (without reboot)
sudo systemctl isolate multi-user.target   # Drop to console
sudo systemctl isolate graphical.target    # Start GUI
```

---

## Fase 6: Target (runlevel)

Una vez que systemd alcanza el target configurado, el sistema está operativo. El target final determina qué servicios están activos.

### graphical.target

```
  ┌──────────────────────────────────────────────────┐
  │              graphical.target                     │
  │                                                  │
  │  Todo lo de multi-user.target MÁS:               │
  │                                                  │
  │  ┌──────────────────────────────────────────┐    │
  │  │ Display Manager (GDM/SDDM/LightDM)      │    │
  │  │  • Presenta pantalla de login gráfico     │    │
  │  │  • Autentica al usuario (PAM)            │    │
  │  │  • Lanza la sesión del escritorio        │    │
  │  └──────────────────────────────────────────┘    │
  │                    │                              │
  │                    ▼                              │
  │  ┌──────────────────────────────────────────┐    │
  │  │ Sesión de usuario                        │    │
  │  │  • Window Manager / Desktop Environment  │    │
  │  │  • systemd --user (servicios de usuario) │    │
  │  │  • Aplicaciones de autostart             │    │
  │  └──────────────────────────────────────────┘    │
  └──────────────────────────────────────────────────┘
```

### multi-user.target (servidores)

```
  ┌──────────────────────────────────────────────────┐
  │             multi-user.target                     │
  │                                                  │
  │  Servicios activos (típicos):                    │
  │  ├── sshd.service         (acceso remoto)        │
  │  ├── NetworkManager       (o systemd-networkd)   │
  │  ├── firewalld            (firewall)             │
  │  ├── chronyd / ntpd       (sincronización hora)  │
  │  ├── rsyslog              (logging)              │
  │  ├── crond                (tareas programadas)   │
  │  ├── auditd               (auditoría)            │
  │  └── getty@tty1-6         (consolas de login)    │
  │                                                  │
  │  ┌──────────────────────────────────────────┐    │
  │  │ getty@tty1.service                       │    │
  │  │  • agetty → muestra "login:"             │    │
  │  │  • Usuario escribe nombre + password     │    │
  │  │  • PAM autentica                         │    │
  │  │  • Lanza shell (bash/zsh)                │    │
  │  └──────────────────────────────────────────┘    │
  └──────────────────────────────────────────────────┘
```

### Login: el paso final

```
  getty (agetty)
       │
       │  Muestra: "hostname login: "
       ▼
  Usuario escribe nombre
       │
       ▼
  login (o PAM)
       │
       │  1. Verificar /etc/passwd (usuario existe)
       │  2. Verificar /etc/shadow (password correcta)
       │  3. Verificar /etc/security/access.conf
       │  4. Aplicar /etc/security/limits.conf
       │  5. Cargar /etc/profile, ~/.bash_profile
       ▼
  Shell interactiva (bash)
       │
       │  El sistema está listo para el usuario
       ▼
  $
```

---

## Línea temporal completa

```
  TIEMPO ═══════════════════════════════════════════════════════════▶

  t=0s          t=1-5s        t=5-7s         t=7-10s
  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
  │ Firmware │  │  GRUB2   │  │  Kernel  │  │initramfs │
  │ POST     │  │  menú +  │  │ decomp + │  │ modules  │
  │ detect   │  │  load    │  │ init HW  │  │ LVM/LUKS │
  │ hardware │  │  kernel  │  │ parsear  │  │ mount /  │
  │          │  │  + initrd│  │ cmdline  │  │ pivot    │
  └──────────┘  └──────────┘  └──────────┘  └──────────┘

  t=10-15s      t=15-25s       t=25s+
  ┌──────────┐  ┌──────────┐  ┌──────────┐
  │ systemd  │  │ Services │  │  Login   │
  │ PID 1    │  │ sshd     │  │  Screen  │
  │ build    │  │ network  │  │          │
  │ dep tree │  │ firewall │  │  Ready!  │
  │          │  │ journal  │  │          │
  └──────────┘  └──────────┘  └──────────┘

  (tiempos aproximados para un servidor típico con SSD)
```

### Todo junto en un diagrama

```
  POWER ON
  │
  ├── Firmware (BIOS/UEFI)
  │   ├── POST (hardware test)
  │   ├── Enumerate devices
  │   └── Find bootloader ──────────────────── MBR(BIOS) o ESP(UEFI)
  │
  ├── GRUB2
  │   ├── Load modules (ext2, lvm, gpt)
  │   ├── Show menu (optional)
  │   ├── Load vmlinuz into RAM
  │   ├── Load initramfs into RAM
  │   └── Pass kernel command line ─────────── root=UUID=... ro quiet
  │
  ├── Kernel
  │   ├── Decompress vmlinuz
  │   ├── Initialize CPU, memory, scheduler
  │   ├── Parse command line
  │   ├── Initialize built-in drivers
  │   ├── Mount initramfs as /
  │   └── Execute /init from initramfs
  │
  ├── initramfs (/init)
  │   ├── Start udev
  │   ├── Load storage modules (nvme, ahci)
  │   ├── Assemble RAID / activate LVM
  │   ├── Open LUKS (prompt for passphrase)
  │   ├── Mount real root on /sysroot
  │   ├── pivot_root (/ = real root)
  │   └── exec /sbin/init (systemd)
  │
  ├── systemd (PID 1)
  │   ├── Read default.target
  │   ├── Build dependency tree
  │   ├── sysinit.target (mount filesystems, swap, hostname)
  │   ├── basic.target (sockets, timers, slices)
  │   ├── multi-user.target (sshd, network, cron, audit)
  │   └── graphical.target (display manager)
  │
  └── Login
      ├── getty → agetty → "login:"
      ├── PAM authentication
      └── Shell (bash) → $
```

---

## Diagnóstico del arranque

### dmesg: mensajes del kernel

```bash
# All kernel messages from boot
dmesg | head -50
# [    0.000000] Linux version 6.7.4-200.fc39.x86_64 ...
# [    0.000000] Command line: BOOT_IMAGE=... root=UUID=... ro quiet
# [    0.000000] BIOS-provided physical RAM map:
# [    0.000000] BIOS-e820: [mem 0x0000000000000000-0x000000000009ffff] usable

# Filter by subsystem
dmesg | grep -i "nvme"       # NVMe disk detection
dmesg | grep -i "usb"        # USB devices
dmesg | grep -i "error"      # Errors during boot
dmesg | grep -i "firmware"   # Firmware messages

# Timestamps in human-readable format
dmesg -T | head -20
# [Sat Mar 15 10:23:45 2026] Linux version 6.7.4 ...
```

### journalctl: log de systemd

```bash
# Current boot log (all messages)
journalctl -b

# Previous boot (useful when current boot fails)
journalctl -b -1

# List all recorded boots
journalctl --list-boots
#  0 abc123 Sat 2026-03-15 10:23:45 — Sat 2026-03-15 18:30:00
# -1 def456 Fri 2026-03-14 08:00:12 — Fri 2026-03-14 23:59:59

# Filter by priority (errors and worse)
journalctl -b -p err

# Filter by unit
journalctl -b -u sshd.service
journalctl -b -u NetworkManager

# Kernel messages only (like dmesg but persistent)
journalctl -b -k
```

### systemd-analyze: tiempos de arranque

```bash
# Total boot time breakdown
systemd-analyze
# Startup finished in 1.234s (firmware) + 2.345s (loader) +
#   1.567s (kernel) + 8.901s (userspace) = 14.047s
# graphical.target reached after 8.901s in userspace.

# Which services took longest
systemd-analyze blame
# 3.456s NetworkManager-wait-online.service
# 1.234s dracut-initqueue.service
# 0.987s firewalld.service
# 0.876s systemd-udev-settle.service
# ...

# Critical path (chain of dependencies that determined boot time)
systemd-analyze critical-chain
# graphical.target @8.901s
# └── display-manager.service @8.500s +400ms
#     └── multi-user.target @8.499s
#         └── sshd.service @5.123s +50ms
#             └── network-online.target @5.100s
#                 └── NetworkManager-wait-online.service @1.644s +3.456s
#                     └── NetworkManager.service @1.500s +144ms

# Generate SVG boot chart
systemd-analyze plot > /tmp/boot-chart.svg

# Show time spent in each boot phase
systemd-analyze time
```

### Persistent journal

Por defecto, en algunas distribuciones el journal no persiste entre reinicios. Para habilitarlo:

```bash
# Check if journal is persistent
ls /var/log/journal/
# If empty or missing → journal is volatile (lost on reboot)

# Enable persistent journal
sudo mkdir -p /var/log/journal
sudo systemd-tmpfiles --create --prefix /var/log/journal
sudo systemctl restart systemd-journald

# Or configure in /etc/systemd/journald.conf:
# [Journal]
# Storage=persistent
```

---

## Errores comunes

### 1. Confundir "el kernel no arranca" con "systemd no arranca"

```
  ┌───────────────────────────────────────────────────────┐
  │  SÍNTOMA                   │  FASE CON PROBLEMA        │
  │                            │                           │
  │  "No bootable device"      │  Firmware (no encuentra   │
  │                            │  bootloader)              │
  │                            │                           │
  │  "error: unknown           │  GRUB (no puede leer      │
  │   filesystem"              │  /boot)                   │
  │                            │                           │
  │  "Kernel panic - not       │  Kernel (no encuentra     │
  │   syncing: VFS: Unable     │  initramfs o root)        │
  │   to mount root fs"        │                           │
  │                            │                           │
  │  "dracut: FATAL: No root   │  initramfs (no encuentra  │
  │   device found"            │  root= device)            │
  │                            │                           │
  │  "Failed to start          │  systemd (servicio roto   │
  │   Something.service"       │  pero sistema arranca)    │
  │                            │                           │
  │  "A start job is running   │  systemd (esperando un    │
  │   for..."  (90s timeout)   │  mount/servicio lento)    │
  └───────────────────────────────────────────────────────┘
```

### 2. No saber que initramfs debe regenerarse al cambiar hardware

Si cambias la controladora de disco (por ejemplo, al migrar una VM de IDE a virtio), el initramfs no contiene el driver nuevo y el sistema no arranca. Solución:

```bash
# Before changing hardware:
sudo dracut --force                    # RHEL: regenerate initramfs
sudo update-initramfs -u               # Debian: regenerate initramfs

# Or from a rescue environment after the fact
```

### 3. Fstab con UUID incorrecto bloquea el arranque

Si `/etc/fstab` referencia un UUID que no existe (disco reemplazado, partición eliminada), systemd espera 90 segundos y luego entra en emergency mode:

```bash
# Fix: boot with systemd.unit=emergency.target
# Then:
mount -o remount,rw /
vim /etc/fstab              # Fix the UUID
# Or comment out the problematic line:
# UUID=old-bad-uuid  /data  ext4  defaults  0 2
systemctl daemon-reload
reboot
```

### 4. Asumir que /sbin/init siempre es systemd

En contenedores y sistemas minimalistas, `/sbin/init` puede no existir o puede ser otro programa. En el examen RHCSA, siempre verifica:

```bash
ls -la /sbin/init
# → ../lib/systemd/systemd   (systemd)
# → busybox                  (container/embedded)
# → /etc/init.d/rcS          (SysVinit)
```

### 5. Ignorar los tiempos de arranque al diagnosticar

```bash
# If boot takes > 30 seconds, ALWAYS check:
systemd-analyze blame | head -10

# Common culprits:
# NetworkManager-wait-online.service  (waiting for DHCP)
# systemd-udev-settle.service         (waiting for slow devices)
# dracut-initqueue.service            (waiting for root device)
# fstab entries with nofail missing   (waiting for absent disks)
```

---

## Cheatsheet

```bash
# ═══════════════════════════════════════════════
#  IDENTIFICAR FASE DE ARRANQUE
# ═══════════════════════════════════════════════
ls /sys/firmware/efi/               # UEFI or BIOS?
cat /proc/cmdline                   # Kernel command line (from GRUB)
dmesg | head -5                     # First kernel messages
systemctl get-default               # Default target

# ═══════════════════════════════════════════════
#  DIAGNÓSTICO DEL KERNEL
# ═══════════════════════════════════════════════
dmesg                               # All kernel messages
dmesg -T                            # With human timestamps
dmesg | grep -i error               # Errors only
journalctl -b -k                    # Kernel log (persistent)

# ═══════════════════════════════════════════════
#  DIAGNÓSTICO DE SYSTEMD
# ═══════════════════════════════════════════════
systemd-analyze                     # Total boot time
systemd-analyze blame               # Slowest services
systemd-analyze critical-chain      # Critical path
systemd-analyze plot > boot.svg     # Visual boot chart

journalctl -b                       # Current boot log
journalctl -b -1                    # Previous boot log
journalctl -b -p err                # Errors this boot
journalctl --list-boots             # All recorded boots

# ═══════════════════════════════════════════════
#  TARGETS
# ═══════════════════════════════════════════════
systemctl get-default               # Show default target
sudo systemctl set-default multi-user.target  # Change default
sudo systemctl isolate rescue.target          # Switch now

# ═══════════════════════════════════════════════
#  INITRAMFS
# ═══════════════════════════════════════════════
lsinitrd /boot/initramfs-$(uname -r).img    # RHEL: list contents
lsinitramfs /boot/initrd.img-$(uname -r)    # Debian: list contents
sudo dracut --force                          # RHEL: regenerate
sudo update-initramfs -u                     # Debian: regenerate

# ═══════════════════════════════════════════════
#  FAILED BOOT RECOVERY
# ═══════════════════════════════════════════════
# From GRUB: add to linux line:
#   systemd.unit=emergency.target    (broken fstab)
#   rd.break                         (forgot root password)
#   nomodeset                        (GPU issues)
#   enforcing=0                      (SELinux issues)
```

---

## Ejercicios

### Ejercicio 1: Trazar el arranque de tu sistema

```bash
# Step 1: identify firmware type
ls /sys/firmware/efi/ 2>/dev/null && echo "UEFI" || echo "BIOS"

# Step 2: check what GRUB loaded
cat /proc/cmdline

# Step 3: see kernel initialization
dmesg | head -30

# Step 4: find when the kernel detected your root disk
dmesg | grep -i -E "sd[a-z]:|nvme|root"

# Step 5: check initramfs contents
lsinitrd /boot/initramfs-$(uname -r).img 2>/dev/null | head -30 || \
    lsinitramfs /boot/initrd.img-$(uname -r) 2>/dev/null | head -30

# Step 6: view systemd boot analysis
systemd-analyze
systemd-analyze blame | head -10

# Step 7: see the critical chain
systemd-analyze critical-chain

# Step 8: check default target
systemctl get-default

# Step 9: list the dependency tree of the default target
systemctl list-dependencies $(systemctl get-default) | head -30
```

**Preguntas:**
1. ¿Cuánto tiempo tomó cada fase (firmware, loader, kernel, userspace)?
2. ¿Qué servicio fue el más lento? ¿Es necesario o se puede optimizar?
3. ¿Cuántos módulos incluye tu initramfs? ¿Reconoces los drivers de tu hardware?

> **Pregunta de predicción**: si tu sistema tiene un SSD NVMe y usas LVM sobre LUKS, ¿qué módulos esperas encontrar en tu initramfs? Verifica tu predicción con `lsinitrd` y busca nvme, dm-mod, dm-crypt.

### Ejercicio 2: Comparar las fases del arranque

```bash
# Step 1: boot normally and record times
systemd-analyze > /tmp/boot_normal.txt
systemd-analyze blame > /tmp/blame_normal.txt

# Step 2: reboot with "quiet" removed
# In GRUB, press 'e', remove "quiet rhgb", Ctrl+X
# After booting, record:
systemd-analyze > /tmp/boot_verbose.txt
systemd-analyze blame > /tmp/blame_verbose.txt

# Step 3: compare
diff /tmp/boot_normal.txt /tmp/boot_verbose.txt
diff /tmp/blame_normal.txt /tmp/blame_verbose.txt

# Step 4: generate a boot chart
systemd-analyze plot > /tmp/boot-chart.svg
# Open in a browser to visualize

# Step 5: investigate the critical chain
systemd-analyze critical-chain graphical.target
# Note which service is the bottleneck

# Step 6: check if persistent journal is enabled
ls /var/log/journal/
journalctl --list-boots | wc -l    # How many boots recorded?
```

**Preguntas:**
1. ¿Quitar `quiet` cambió el tiempo total de arranque? ¿Por qué o por qué no?
2. ¿Cuál es el "cuello de botella" en la critical chain?
3. ¿Cuántos boots tiene registrados tu journal?

> **Pregunta de predicción**: si deshabilitas `NetworkManager-wait-online.service`, ¿el arranque será más rápido? ¿Qué servicios podrían fallar por arrancar sin red configurada?

### Ejercicio 3: Simular un arranque roto y repararlo

> **Hazlo SOLO en una VM con snapshot.**

```bash
# PREPARATION: take a VM snapshot!

# Scenario: corrupt fstab (add a non-existent mount)
sudo cp /etc/fstab /etc/fstab.bak
echo "UUID=00000000-0000-0000-0000-000000000000 /broken ext4 defaults 0 2" | \
    sudo tee -a /etc/fstab

# Reboot the VM
sudo reboot

# EXPECTED: systemd waits ~90 seconds then drops to emergency mode
# OR: shows "A start job is running for /broken" message

# REPAIR (in emergency mode):
# 1. Enter root password when prompted
# 2. Remount root as read-write
mount -o remount,rw /

# 3. Fix fstab
vim /etc/fstab
# Remove the line with UUID=00000000-...
# Or restore backup:
cp /etc/fstab.bak /etc/fstab

# 4. Reload and reboot
systemctl daemon-reload
reboot

# ALTERNATIVE REPAIR: from GRUB
# 1. Press 'e' in GRUB menu
# 2. Add: systemd.unit=emergency.target
# 3. Ctrl+X
# 4. Fix fstab as above
```

**Preguntas:**
1. ¿Cuánto tiempo esperó systemd antes de entrar en emergency mode?
2. ¿Qué hubiera pasado si la línea de fstab tuviera la opción `nofail`?
3. ¿Por qué systemd no puede simplemente ignorar los mounts que fallan?

> **Pregunta de predicción**: si en lugar de un UUID falso, hubieras puesto una entrada para `/` con un UUID incorrecto (reemplazando la entrada real de root), ¿el sistema habría llegado siquiera a systemd? ¿O se habría detenido en una fase anterior?

---

> **Siguiente tema**: S02 — Initramfs, T01 — Qué es initramfs: para qué sirve, cuándo se necesita regenerar.
