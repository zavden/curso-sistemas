# /proc y /sys en profundidad

## Índice
1. [Pseudo-filesystems: la filosofía](#pseudo-filesystems-la-filosofía)
2. [/proc: información del kernel y procesos](#proc-información-del-kernel-y-procesos)
3. [/proc/PID: introspección de procesos](#procpid-introspección-de-procesos)
4. [/proc: archivos globales del kernel](#proc-archivos-globales-del-kernel)
5. [/sys: el modelo de dispositivos](#sys-el-modelo-de-dispositivos)
6. [/sys/class: dispositivos por tipo](#sysclass-dispositivos-por-tipo)
7. [/sys/block: dispositivos de bloque](#sysblock-dispositivos-de-bloque)
8. [/sys/module: módulos del kernel](#sysmodule-módulos-del-kernel)
9. [/sys para tuning de red y scheduler](#sys-para-tuning-de-red-y-scheduler)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## Pseudo-filesystems: la filosofía

Linux sigue la filosofía "todo es un archivo". `/proc` y `/sys` son **pseudo-filesystems** — no existen en disco, sino que el kernel los genera en memoria para exponer su estado interno como archivos legibles y, en muchos casos, escribibles.

```
  ┌──────────────────────────────────────────────────────────────┐
  │                                                              │
  │  Filesystem real          Pseudo-filesystems                 │
  │  (ext4, xfs)              (procfs, sysfs)                    │
  │                                                              │
  │  ┌──────────┐             ┌──────────┐   ┌──────────┐       │
  │  │  Disco   │             │  /proc   │   │  /sys    │       │
  │  │  datos   │             │  Kernel  │   │  Device  │       │
  │  │  reales  │             │  info +  │   │  model + │       │
  │  │          │             │  process │   │  tuning  │       │
  │  │          │             │  info    │   │          │       │
  │  └──────────┘             └──────────┘   └──────────┘       │
  │  Persiste en disco        Solo en RAM    Solo en RAM         │
  │  Datos del usuario        Generado por   Generado por        │
  │                           el kernel      el kernel           │
  │                           (procfs)       (sysfs)             │
  └──────────────────────────────────────────────────────────────┘
```

### /proc vs /sys: roles complementarios

| | /proc | /sys |
|---|---|---|
| **Nombre** | procfs | sysfs |
| **Introducido** | Linux 1.0 (1994) | Linux 2.6 (2004) |
| **Propósito original** | Información de procesos | Modelo de dispositivos |
| **Contenido** | Procesos + estado global del kernel | Dispositivos, drivers, módulos, buses |
| **Organización** | Mixta (creció orgánicamente) | Jerárquica y estructurada |
| **Tuning** | `/proc/sys/` (vía sysctl) | Parámetros de dispositivos y drivers |
| **Montaje** | `mount -t proc proc /proc` | `mount -t sysfs sysfs /sys` |

> **Historia**: `/proc` creció desordenadamente con los años — empezó como información de procesos pero acabó siendo el cajón de sastre del kernel. `/sys` nació en 2.6 para organizar la información de dispositivos de forma limpia. Hoy coexisten, con algo de solapamiento.

---

## /proc: información del kernel y procesos

```
  /proc/
  ├── 1/                   ← Información del proceso PID 1 (systemd)
  ├── 2/                   ← PID 2 (kthreadd)
  ├── ...
  ├── <PID>/               ← Un directorio por cada proceso activo
  │
  ├── cmdline              ← Línea de comandos del kernel (de GRUB)
  ├── cpuinfo              ← Información detallada del CPU
  ├── meminfo              ← Estado de la memoria
  ├── version              ← Versión del kernel
  ├── uptime               ← Tiempo encendido
  ├── loadavg              ← Carga promedio
  ├── stat                 ← Estadísticas globales del kernel
  ├── mounts               ← Filesystems montados
  ├── partitions            ← Tabla de particiones
  ├── modules              ← Módulos cargados (fuente de lsmod)
  ├── interrupts            ← Contadores de interrupciones por CPU
  ├── filesystems           ← Filesystems soportados
  ├── swaps                 ← Swap activo
  ├── diskstats             ← Estadísticas de I/O de discos
  ├── net/                  ← Información de red
  │   ├── dev               ← Estadísticas por interfaz
  │   ├── tcp               ← Conexiones TCP activas
  │   ├── udp               ← Sockets UDP
  │   ├── arp               ← Tabla ARP
  │   └── route             ← Tabla de rutas
  │
  └── sys/                  ← Parámetros tuneables (sysctl)
      ├── kernel/
      ├── net/
      ├── vm/
      └── fs/
```

---

## /proc/PID: introspección de procesos

Cada proceso en ejecución tiene un directorio `/proc/<PID>/` con información exhaustiva:

```bash
# Pick a process (e.g., PID 1 = systemd)
ls /proc/1/
```

### Archivos más útiles

```bash
PID=1  # Change to any PID

# ── Identidad ──
cat /proc/$PID/comm          # Nombre corto del comando
# systemd
cat /proc/$PID/cmdline | tr '\0' ' '   # Línea de comandos completa
# /usr/lib/systemd/systemd --switched-root --system --deserialize 31
cat /proc/$PID/exe           # Symlink al binario
readlink /proc/$PID/exe
# /usr/lib/systemd/systemd

# ── Estado ──
cat /proc/$PID/status        # Estado legible
# Name:   systemd
# Umask:  0000
# State:  S (sleeping)
# Tgid:   1
# Pid:    1
# PPid:   0
# Uid:    0  0  0  0
# Gid:    0  0  0  0
# VmPeak: 256000 kB
# VmRSS:  12000 kB
# Threads: 1
# ...

cat /proc/$PID/stat          # Estado en formato máquina (una línea)
cat /proc/$PID/statm         # Memoria en páginas

# ── Memoria ──
cat /proc/$PID/maps          # Mapa de memoria (regiones mapeadas)
# address           perms offset  dev   inode  pathname
# 55a7a8e00000-55a7a9000000 r-xp ... /usr/lib/systemd/systemd
# 7f1234560000-7f1234780000 r-xp ... /usr/lib/libc.so.6

cat /proc/$PID/smaps_rollup  # Resumen de uso de memoria
# Rss:       12000 kB
# Pss:       10000 kB     ← Proportional Set Size (RAM real)
# Shared_Clean:  5000 kB
# Private_Dirty: 3000 kB

# ── Filesystem ──
ls -la /proc/$PID/fd/        # File descriptors abiertos
# lrwx------ 0 -> /dev/null
# lrwx------ 1 -> /dev/null
# lr-x------ 3 -> /proc/1/mountinfo
# lrwx------ 8 -> socket:[12345]

cat /proc/$PID/fdinfo/3      # Info detallada de fd 3
readlink /proc/$PID/cwd      # Directorio de trabajo
readlink /proc/$PID/root     # Root del filesystem (chroot)

# ── Entorno ──
cat /proc/$PID/environ | tr '\0' '\n' | head -5
# HOME=/
# PATH=/usr/local/sbin:...
# LANG=en_US.UTF-8

# ── Límites ──
cat /proc/$PID/limits
# Limit                     Soft Limit  Hard Limit  Units
# Max open files            1024        524288      files
# Max processes             31218       31218       processes
# Max file size             unlimited   unlimited   bytes

# ── Control groups ──
cat /proc/$PID/cgroup
# 0::/init.scope

# ── Scheduling ──
cat /proc/$PID/sched | head -10
# systemd (1, #threads: 1)
# se.exec_start           :      12345678.901234
# policy                  :             0 (SCHED_NORMAL)
# prio                    :           120
```

### /proc/self: el proceso actual

```bash
# /proc/self is a symlink to /proc/<PID of current process>
readlink /proc/self
# 12345  (PID of the shell reading this)

cat /proc/self/comm
# bash (or zsh, the shell running cat)

# Useful for scripts that need to inspect themselves:
cat /proc/self/limits | grep "open files"
```

---

## /proc: archivos globales del kernel

### Hardware y recursos

```bash
# ── CPU ──
cat /proc/cpuinfo | head -25
# processor  : 0
# vendor_id  : GenuineIntel
# model name : Intel(R) Core(TM) i7-10750H CPU @ 2.60GHz
# cpu MHz    : 2592.000
# cache size : 12288 KB
# cpu cores  : 6
# flags      : fpu vme de ... aes avx avx2 ...
# bugs       : spectre_v1 spectre_v2 mds ...

# Quick: count CPUs
grep -c "^processor" /proc/cpuinfo
# 12

# Check for specific CPU feature
grep -o "aes" /proc/cpuinfo | head -1
# aes   (AES-NI hardware acceleration present)

# ── Memoria ──
cat /proc/meminfo | head -15
# MemTotal:       32768000 kB
# MemFree:         8000000 kB
# MemAvailable:   20000000 kB    ← Realmente disponible
# Buffers:          500000 kB
# Cached:         10000000 kB
# SwapTotal:       8388604 kB
# SwapFree:        8388604 kB
# Dirty:              1234 kB    ← Pendiente de escribir a disco

# ── Interrupts ──
cat /proc/interrupts | head -5
#            CPU0   CPU1   CPU2   ...
#   0:         50      0      0   IO-APIC   2-edge    timer
#   1:          0      0   1234   IO-APIC   1-edge    i8042
#   8:          0      1      0   IO-APIC   8-edge    rtc0

# ── Uptime ──
cat /proc/uptime
# 123456.78 987654.32
# (seconds up) (seconds idle across all CPUs)

# ── Load average ──
cat /proc/loadavg
# 0.50 0.75 1.00 2/350 12345
# (1min  5min  15min  running/total  last_pid)
```

### Red

```bash
# ── Interface statistics ──
cat /proc/net/dev
# Inter-|   Receive                  |  Transmit
#  face |bytes  packets errs drop ...|bytes  packets errs drop ...
#  eth0: 123456  1000    0    0   ... 654321  800     0    0

# ── Active TCP connections ──
cat /proc/net/tcp | head -3
#   sl  local_address rem_address   st tx_queue rx_queue ...
#    0: 0100007F:0035 00000000:0000 0A 00000000:00000000 ...
# (Addresses are in hex, reversed byte order)
# Alternative: ss or netstat for human-readable

# ── ARP table ──
cat /proc/net/arp
# IP address    HW type  Flags  HW address          Mask  Device
# 192.168.1.1   0x1      0x2    aa:bb:cc:dd:ee:ff   *     eth0

# ── Routing table ──
cat /proc/net/route
# Iface  Destination  Gateway  Flags  RefCnt  Use  Metric  Mask  ...
# (hex values — use "ip route" for human-readable)
```

### Kernel

```bash
# ── Kernel version ──
cat /proc/version
# Linux version 6.7.4-200.fc39.x86_64 (mockbuild@...) (gcc 13.2.1) #1 SMP ...

# ── Kernel command line (from GRUB) ──
cat /proc/cmdline
# BOOT_IMAGE=(hd0,gpt2)/vmlinuz-6.7.4 root=UUID=... ro rhgb quiet

# ── Supported filesystems ──
cat /proc/filesystems
# nodev  sysfs
# nodev  tmpfs
# nodev  proc
#        ext4
#        xfs
# "nodev" = pseudo-filesystem (doesn't use a block device)

# ── Loaded modules (source for lsmod) ──
cat /proc/modules | head -3
# ext4 999424 1 - Live 0xffffffff... (E)

# ── Partition table ──
cat /proc/partitions
# major minor  #blocks  name
#  259     0  500107608 nvme0n1
#  259     1     614400 nvme0n1p1
#  259     2    1048576 nvme0n1p2
#  259     3  498444232 nvme0n1p3
```

---

## /sys: el modelo de dispositivos

`/sys` (sysfs) presenta la visión del kernel sobre todos los dispositivos, drivers, buses y módulos del sistema, organizado de forma jerárquica.

```
  /sys/
  ├── block/          ← Dispositivos de bloque (sda, nvme0n1)
  ├── bus/            ← Buses del sistema
  │   ├── pci/        ← Dispositivos PCI
  │   ├── usb/        ← Dispositivos USB
  │   ├── scsi/       ← Dispositivos SCSI
  │   └── ...
  ├── class/          ← Dispositivos organizados por tipo
  │   ├── net/        ← Interfaces de red
  │   ├── block/      ← Block devices
  │   ├── tty/        ← Terminales
  │   ├── input/      ← Dispositivos de entrada
  │   └── ...
  ├── devices/        ← Todos los dispositivos (jerarquía por bus)
  │   ├── system/     ← CPU, memoria, nodos
  │   ├── pci0000:00/ ← PCI bus root
  │   └── virtual/    ← Dispositivos virtuales
  ├── firmware/       ← Interfaces del firmware
  │   ├── efi/        ← Variables UEFI
  │   ├── acpi/       ← Tablas ACPI
  │   └── dmi/        ← SMBIOS/DMI
  ├── fs/             ← Filesystems (cgroups, ext4, etc.)
  ├── kernel/         ← Objetos del kernel
  │   ├── mm/         ← Memory management
  │   └── security/   ← LSM (SELinux, AppArmor)
  ├── module/         ← Módulos cargados (parámetros)
  └── power/          ← Gestión de energía
```

### Principio de "un valor por archivo"

A diferencia de `/proc` que a veces empaqueta múltiples valores en un archivo, `/sys` sigue la regla de **un valor por archivo**:

```bash
# /proc style: multiple values in one file
cat /proc/meminfo | head -3
# MemTotal:       32768000 kB
# MemFree:         8000000 kB
# MemAvailable:   20000000 kB

# /sys style: one value per file
cat /sys/block/sda/size
# 976773168
cat /sys/block/sda/queue/scheduler
# [mq-deadline] kyber bfq none
```

---

## /sys/class: dispositivos por tipo

`/sys/class/` organiza los dispositivos por su **función**, independientemente de cómo están conectados físicamente:

### Red

```bash
# List all network interfaces
ls /sys/class/net/
# enp3s0  lo  virbr0  docker0

# Interface details
cat /sys/class/net/enp3s0/address          # MAC address
# aa:bb:cc:dd:ee:ff
cat /sys/class/net/enp3s0/mtu              # MTU
# 1500
cat /sys/class/net/enp3s0/speed            # Speed in Mbps
# 1000
cat /sys/class/net/enp3s0/operstate        # up/down
# up
cat /sys/class/net/enp3s0/carrier          # Link detected
# 1
cat /sys/class/net/enp3s0/type             # 1=Ethernet, 772=loopback
# 1

# Which driver handles this interface
basename $(readlink /sys/class/net/enp3s0/device/driver/module)
# e1000e

# Statistics
cat /sys/class/net/enp3s0/statistics/rx_bytes
# 123456789
cat /sys/class/net/enp3s0/statistics/tx_bytes
# 987654321
cat /sys/class/net/enp3s0/statistics/rx_errors
# 0
```

### Otros tipos útiles

```bash
# ── Thermal zones (temperature) ──
ls /sys/class/thermal/
cat /sys/class/thermal/thermal_zone0/type
# x86_pkg_temp
cat /sys/class/thermal/thermal_zone0/temp
# 45000   (millidegrees Celsius → 45.0°C)

# ── Power supply (battery) ──
cat /sys/class/power_supply/BAT0/capacity     # Battery %
cat /sys/class/power_supply/BAT0/status       # Charging/Discharging
cat /sys/class/power_supply/AC/online         # 1 = plugged in

# ── Backlight (brightness) ──
cat /sys/class/backlight/intel_backlight/brightness
cat /sys/class/backlight/intel_backlight/max_brightness
# Write to change: echo 500 > brightness

# ── LEDs ──
ls /sys/class/leds/
# input3::capslock  input3::numlock  input3::scrolllock

# ── TTY (terminals) ──
ls /sys/class/tty/ | head -5
# console  tty0  tty1  ttyS0
```

---

## /sys/block: dispositivos de bloque

```bash
# List block devices
ls /sys/block/
# nvme0n1  sda  dm-0  dm-1  loop0  loop1

# ── Disk information ──
cat /sys/block/nvme0n1/size               # Size in 512-byte sectors
# 976773168  (× 512 = 500 GB)

cat /sys/block/nvme0n1/queue/rotational   # 0 = SSD, 1 = HDD
# 0

cat /sys/block/nvme0n1/device/model       # Disk model
# Samsung SSD 970 EVO Plus

cat /sys/block/nvme0n1/device/serial       # Serial number (if available)

# ── Partitions ──
ls /sys/block/nvme0n1/nvme0n1p*/size
# /sys/block/nvme0n1/nvme0n1p1/size → 1228800
# /sys/block/nvme0n1/nvme0n1p2/size → 2097152
# /sys/block/nvme0n1/nvme0n1p3/size → 996888464

# ── I/O Scheduler ──
cat /sys/block/nvme0n1/queue/scheduler
# [none] mq-deadline kyber bfq
# [brackets] = current scheduler

# Change scheduler (temporary)
echo mq-deadline | sudo tee /sys/block/nvme0n1/queue/scheduler

# ── Queue parameters ──
cat /sys/block/nvme0n1/queue/nr_requests       # Max I/O queue depth
cat /sys/block/nvme0n1/queue/read_ahead_kb     # Read-ahead in KB
cat /sys/block/nvme0n1/queue/max_sectors_kb     # Max I/O size
```

### I/O Schedulers

```
  ┌────────────────────────────────────────────────────────────┐
  │  SCHEDULER    │  DESCRIPCIÓN             │  MEJOR PARA     │
  │───────────────┼──────────────────────────┼─────────────────│
  │  none         │  Sin scheduling (FIFO)   │  NVMe, VMs      │
  │               │  Mínima latencia          │  (hardware ya   │
  │               │                          │   tiene colas)   │
  │───────────────┼──────────────────────────┼─────────────────│
  │  mq-deadline  │  Merge + deadline        │  HDD, DB,       │
  │               │  Garantiza latencia máx   │  cargas mixtas  │
  │───────────────┼──────────────────────────┼─────────────────│
  │  bfq          │  Budget Fair Queueing    │  Desktop, SSD   │
  │               │  Fairness per-process    │  interactivo    │
  │───────────────┼──────────────────────────┼─────────────────│
  │  kyber        │  Latency-oriented        │  Servidores     │
  │               │  Lightweight, low CPU    │  con SSD        │
  └────────────────────────────────────────────────────────────┘
```

---

## /sys/module: módulos del kernel

```bash
# List all kernel modules (loaded + built-in with /sys exposure)
ls /sys/module/ | head -10

# ── Module information ──
cat /sys/module/ext4/initstate
# live   (loaded as module)

# ── Module parameters (readable/writable) ──
ls /sys/module/ext4/parameters/
cat /sys/module/ext4/parameters/mb_debug_v2
# 0

# ── Change parameter at runtime (if writable) ──
ls -la /sys/module/nf_conntrack/parameters/
# -r--r--r-- hashsize       ← Read-only
# -rw-r--r-- nf_conntrack_max  ← Writable!

# View and change conntrack max
cat /sys/module/nf_conntrack/parameters/nf_conntrack_max
# 262144
echo 524288 | sudo tee /sys/module/nf_conntrack/parameters/nf_conntrack_max

# ── Driver version ──
cat /sys/module/e1000e/version 2>/dev/null
```

---

## /sys para tuning de red y scheduler

### Tuning de red vía /sys

```bash
# ── Offloading (hardware acceleration) ──
# View offload status (requires ethtool)
ethtool -k enp3s0 | head -10
# tx-checksumming: on
# rx-checksumming: on
# scatter-gather: on
# tcp-segmentation-offload: on
# generic-receive-offload: on

# ── Ring buffer size (NIC queues) ──
ethtool -g enp3s0
# Pre-set maximums:
# RX:    4096
# TX:    4096
# Current hardware settings:
# RX:    256
# TX:    256

# Increase ring buffer (reduce drops under load)
sudo ethtool -G enp3s0 rx 4096 tx 4096

# ── IRQ affinity (bind NIC interrupts to specific CPUs) ──
# Find the IRQ for your NIC
grep enp3s0 /proc/interrupts
#  37:  12345  0  0  0  IR-PCI-MSI  enp3s0-TxRx-0

# View current CPU affinity
cat /proc/irq/37/smp_affinity
# f  (bitmask: all CPUs)

# Pin to CPU 0 only
echo 1 | sudo tee /proc/irq/37/smp_affinity

# ── Network queue discipline ──
cat /sys/class/net/enp3s0/queues/tx-0/xps_cpus  # Transmit CPU steering
```

### Tuning del scheduler de CPU vía /proc y /sys

```bash
# ── Per-process scheduling ──
# View scheduling info for PID 1
cat /proc/1/sched | head -5
# systemd (1, #threads: 1)
# policy: 0 (SCHED_NORMAL)
# prio: 120

# ── Scheduler tunables ──
ls /proc/sys/kernel/sched_*
# sched_child_runs_first
# sched_min_granularity_ns
# sched_wakeup_granularity_ns
# sched_latency_ns
# ...

# Reduce scheduler latency for interactive workloads
cat /proc/sys/kernel/sched_latency_ns
# 6000000  (6ms)
cat /proc/sys/kernel/sched_min_granularity_ns
# 750000   (0.75ms)

# ── CPU frequency ──
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
# powersave / performance / schedutil

# Change governor (all CPUs)
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo performance | sudo tee $cpu
done

# View available governors
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors
# conservative ondemand userspace powersave performance schedutil

# ── CPU topology ──
# Physical cores
cat /sys/devices/system/cpu/cpu0/topology/core_id
# Thread siblings (hyperthreading pairs)
cat /sys/devices/system/cpu/cpu0/topology/thread_siblings_list
# 0,6   (CPU 0 and CPU 6 are hyperthreading siblings)

# ── NUMA nodes ──
ls /sys/devices/system/node/
# node0  node1  (on multi-socket systems)
cat /sys/devices/system/node/node0/meminfo | head -5
```

### Tuning del scheduler de I/O

```bash
# ── View current I/O scheduler per disk ──
for disk in /sys/block/*/queue/scheduler; do
    echo "$disk: $(cat $disk)"
done
# /sys/block/nvme0n1/queue/scheduler: [none] mq-deadline kyber bfq
# /sys/block/sda/queue/scheduler: [mq-deadline] kyber bfq none

# ── Change scheduler ──
echo mq-deadline | sudo tee /sys/block/sda/queue/scheduler

# ── Persistent via udev rule ──
# /etc/udev/rules.d/60-io-scheduler.rules
# ACTION=="add|change", KERNEL=="sd[a-z]", ATTR{queue/rotational}=="1", \
#   ATTR{queue/scheduler}="mq-deadline"
# ACTION=="add|change", KERNEL=="nvme[0-9]*", ATTR{queue/rotational}=="0", \
#   ATTR{queue/scheduler}="none"

# ── Read-ahead tuning ──
cat /sys/block/sda/queue/read_ahead_kb
# 128  (default)
# Increase for sequential workloads (databases, streaming):
echo 2048 | sudo tee /sys/block/sda/queue/read_ahead_kb
# Decrease for random I/O (SSDs):
echo 64 | sudo tee /sys/block/nvme0n1/queue/read_ahead_kb
```

---

## Errores comunes

### 1. Confundir /proc/sys (tuneable) con /proc (informativo)

```bash
# /proc/sys/ → tunable via sysctl, writable
echo 1 | sudo tee /proc/sys/net/ipv4/ip_forward    # Works

# /proc/cpuinfo → read-only information, NOT writable
echo "fast" > /proc/cpuinfo    # ERROR: Permission denied (and pointless)
```

No todo en `/proc` es configurable. Solo `/proc/sys/` contiene parámetros tuneables. El resto es información de solo lectura.

### 2. Escribir en /sys sin verificar permisos

```bash
# Check before writing
ls -la /sys/block/sda/queue/scheduler
# -rw-r--r-- → writable by root

ls -la /sys/block/sda/size
# -r--r--r-- → read-only (can't change disk size!)
```

### 3. Asumir que cambios en /sys persisten

Todos los cambios en `/sys` son **temporales** — se pierden al reiniciar. Para persistir:

```bash
# ── I/O scheduler → udev rule ──
# /etc/udev/rules.d/60-io-scheduler.rules

# ── CPU governor → tuned profile or systemd service ──
# Use tuned: sudo tuned-adm profile throughput-performance

# ── Sysctl params → /etc/sysctl.d/*.conf ──

# ── Module params → /etc/modprobe.d/*.conf ──
```

### 4. Usar /proc/net/* en lugar de herramientas modernas

```bash
# /proc/net/tcp is hex and hard to read
cat /proc/net/tcp | head -3
# sl local_address rem_address st ...
#  0: 0100007F:0035 00000000:0000 0A ...

# Use ss (modern) or netstat instead:
ss -tlnp                # Much more readable
```

### 5. No saber que /proc/PID desaparece con el proceso

```bash
# If PID 12345 exits, /proc/12345/ vanishes instantly
cat /proc/12345/status
# cat: /proc/12345/status: No such file or directory
# (process already terminated)
```

---

## Cheatsheet

```bash
# ═══════════════════════════════════════════════
#  /proc — PROCESOS
# ═══════════════════════════════════════════════
cat /proc/<PID>/status              # Process state (readable)
cat /proc/<PID>/cmdline | tr '\0' ' '  # Command line
readlink /proc/<PID>/exe            # Binary path
ls -la /proc/<PID>/fd/              # Open file descriptors
cat /proc/<PID>/maps                # Memory map
cat /proc/<PID>/environ | tr '\0' '\n'  # Environment
cat /proc/<PID>/limits              # Resource limits
cat /proc/self/status               # Current process

# ═══════════════════════════════════════════════
#  /proc — SISTEMA
# ═══════════════════════════════════════════════
cat /proc/cpuinfo                   # CPU details
grep -c "^processor" /proc/cpuinfo  # CPU count
cat /proc/meminfo                   # Memory state
cat /proc/cmdline                   # Kernel command line
cat /proc/version                   # Kernel version
cat /proc/uptime                    # Uptime (seconds)
cat /proc/loadavg                   # Load average
cat /proc/mounts                    # Mounted filesystems
cat /proc/partitions                # Partition table
cat /proc/swaps                     # Active swap
cat /proc/interrupts                # IRQ counters
cat /proc/diskstats                 # Disk I/O stats

# ═══════════════════════════════════════════════
#  /sys — RED
# ═══════════════════════════════════════════════
ls /sys/class/net/                   # List interfaces
cat /sys/class/net/<if>/address      # MAC
cat /sys/class/net/<if>/mtu          # MTU
cat /sys/class/net/<if>/speed        # Speed (Mbps)
cat /sys/class/net/<if>/operstate    # up/down
cat /sys/class/net/<if>/statistics/rx_bytes  # RX bytes

# ═══════════════════════════════════════════════
#  /sys — DISCOS
# ═══════════════════════════════════════════════
ls /sys/block/                       # List block devices
cat /sys/block/<disk>/size           # Size in sectors
cat /sys/block/<disk>/queue/rotational    # 0=SSD, 1=HDD
cat /sys/block/<disk>/queue/scheduler     # I/O scheduler
echo <sched> | sudo tee /sys/block/<disk>/queue/scheduler

# ═══════════════════════════════════════════════
#  /sys — MÓDULOS Y CPU
# ═══════════════════════════════════════════════
ls /sys/module/<mod>/parameters/     # Module params
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
cat /sys/class/thermal/thermal_zone0/temp   # CPU temp (m°C)

# ═══════════════════════════════════════════════
#  PERSISTIR CAMBIOS DE /sys
# ═══════════════════════════════════════════════
# I/O scheduler: /etc/udev/rules.d/60-io-scheduler.rules
# CPU governor: tuned-adm profile <profile>
# Module params: /etc/modprobe.d/<name>.conf
# Sysctl: /etc/sysctl.d/<name>.conf
```

---

## Ejercicios

### Ejercicio 1: Inspeccionar un proceso vía /proc

```bash
# Step 1: find a long-running process
PID=$(pgrep -x sshd | head -1 || pgrep -x systemd | head -1)
echo "Inspecting PID: $PID"

# Step 2: identity
echo "=== Identity ==="
echo "Comm: $(cat /proc/$PID/comm)"
echo "Exe: $(readlink /proc/$PID/exe)"
echo "Cmdline: $(cat /proc/$PID/cmdline | tr '\0' ' ')"

# Step 3: state and resources
echo "=== State ==="
grep -E "^(Name|State|Pid|PPid|Uid|VmRSS|Threads)" /proc/$PID/status

# Step 4: open files
echo "=== Open FDs ==="
ls /proc/$PID/fd/ 2>/dev/null | wc -l
echo "file descriptors open"

# Step 5: memory map summary
echo "=== Memory ==="
cat /proc/$PID/smaps_rollup 2>/dev/null | grep -E "^(Rss|Pss|Shared|Private)"

# Step 6: limits
echo "=== Limits ==="
grep "open files" /proc/$PID/limits

# Step 7: cgroup
echo "=== Cgroup ==="
cat /proc/$PID/cgroup

# Step 8: compare with ps output
echo "=== ps comparison ==="
ps -p $PID -o pid,ppid,user,rss,vsz,comm
```

**Preguntas:**
1. ¿Cuántos file descriptors tiene abiertos el proceso? ¿Qué tipos son (archivos, sockets, pipes)?
2. ¿Cuál es la diferencia entre RSS y PSS en `smaps_rollup`?
3. ¿El valor de RSS de `/proc/PID/status` coincide con lo que reporta `ps`?

> **Pregunta de predicción**: si el proceso inspecccionado se cierra mientras ejecutas los comandos, ¿qué pasará con `/proc/PID/`? ¿Los comandos siguientes darán error o datos vacíos?

### Ejercicio 2: Descubrir hardware vía /sys

```bash
# Step 1: identify disk type (SSD vs HDD)
echo "=== Disks ==="
for disk in /sys/block/sd* /sys/block/nvme* 2>/dev/null; do
    name=$(basename $disk)
    rotational=$(cat $disk/queue/rotational 2>/dev/null)
    scheduler=$(cat $disk/queue/scheduler 2>/dev/null)
    size_sectors=$(cat $disk/size 2>/dev/null)
    size_gb=$(( size_sectors * 512 / 1024 / 1024 / 1024 ))
    echo "  $name: ${size_gb}GB, rotational=$rotational, scheduler=[$scheduler]"
done

# Step 2: network interfaces
echo "=== Network ==="
for iface in /sys/class/net/*; do
    name=$(basename $iface)
    mac=$(cat $iface/address)
    state=$(cat $iface/operstate)
    speed=$(cat $iface/speed 2>/dev/null || echo "N/A")
    driver=$(basename $(readlink $iface/device/driver/module 2>/dev/null) 2>/dev/null || echo "virtual")
    echo "  $name: $mac, state=$state, speed=${speed}Mbps, driver=$driver"
done

# Step 3: CPU info
echo "=== CPU ==="
echo "  Model: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_driver 2>/dev/null || echo 'N/A')"
echo "  Governor: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo 'N/A')"
echo "  CPUs online: $(cat /sys/devices/system/cpu/online)"

# Step 4: temperature
echo "=== Temperature ==="
for tz in /sys/class/thermal/thermal_zone*; do
    type=$(cat $tz/type)
    temp=$(cat $tz/temp 2>/dev/null)
    if [ -n "$temp" ]; then
        echo "  $type: $(echo "scale=1; $temp/1000" | bc)°C"
    fi
done

# Step 5: NUMA topology (if multi-socket)
echo "=== NUMA ==="
ls /sys/devices/system/node/ 2>/dev/null | grep node
```

**Preguntas:**
1. ¿Tus discos son SSD o HDD? ¿Qué scheduler usa cada uno? ¿Es el correcto?
2. ¿Cuántas interfaces de red tienes? ¿Cuáles son virtuales (sin driver de hardware)?
3. ¿Cuál es la temperatura actual de tu CPU?

> **Pregunta de predicción**: si cambias el I/O scheduler de un disco NVMe de `none` a `mq-deadline` con `echo mq-deadline > /sys/block/nvme0n1/queue/scheduler`, ¿notarás alguna diferencia de rendimiento? ¿El cambio sobrevivirá un reinicio?

### Ejercicio 3: Comparar /proc y /sys para la misma información

```bash
# Several pieces of information are accessible from BOTH /proc and /sys
# Let's compare:

# Step 1: module list
echo "=== Modules: /proc vs /sys ==="
echo "Modules in /proc/modules: $(wc -l < /proc/modules)"
echo "Modules in /sys/module/: $(ls /sys/module/ | wc -l)"
echo "(Difference: /sys/module includes built-in modules too)"

# Step 2: network interface stats
IFACE=$(ip route | awk '/default/{print $5}' | head -1)
echo "=== Stats for $IFACE ==="
echo "From /proc/net/dev:"
grep $IFACE /proc/net/dev | awk '{print "  RX bytes:", $2, "TX bytes:", $10}'
echo "From /sys/class/net/$IFACE/statistics:"
echo "  RX bytes: $(cat /sys/class/net/$IFACE/statistics/rx_bytes)"
echo "  TX bytes: $(cat /sys/class/net/$IFACE/statistics/tx_bytes)"

# Step 3: disk partitions
echo "=== Partitions ==="
echo "From /proc/partitions:"
cat /proc/partitions | tail -n +3 | awk '{print "  " $4 ": " $3 " blocks"}'
echo "From /sys/block/:"
for disk in /sys/block/*/; do
    name=$(basename $disk)
    [[ "$name" == loop* ]] && continue
    echo "  $name: $(cat $disk/size) sectors"
done

# Step 4: CPU count
echo "=== CPU count ==="
echo "From /proc/cpuinfo: $(grep -c '^processor' /proc/cpuinfo)"
echo "From /sys/devices/system/cpu/online: $(cat /sys/devices/system/cpu/online)"

# Step 5: uptime comparison
echo "=== Uptime ==="
echo "From /proc/uptime: $(cat /proc/uptime | awk '{print $1}') seconds"
echo "From uptime command: $(uptime -p)"
```

**Preguntas:**
1. ¿Por qué `/sys/module/` tiene más entradas que `/proc/modules`?
2. ¿Los valores de RX/TX bytes coinciden entre `/proc/net/dev` y `/sys/class/net/`?
3. ¿Cuándo usarías `/proc` vs `/sys` para obtener información de red?

> **Pregunta de predicción**: si escribes un script de monitorización que lee `/proc/meminfo` cada segundo, ¿esto genera I/O real al disco o carga significativa en el sistema? ¿Qué overhead tiene leer un pseudo-filesystem?

---

> **Siguiente tema**: T03 — Compilación del kernel: descargar, .config, make menuconfig, make, make modules_install, make install.
