# sysctl

## Índice
1. [¿Qué es sysctl?](#qué-es-sysctl)
2. [/proc/sys: la interfaz del kernel](#procsys-la-interfaz-del-kernel)
3. [Leer parámetros](#leer-parámetros)
4. [Modificar parámetros en caliente](#modificar-parámetros-en-caliente)
5. [Configuración persistente](#configuración-persistente)
6. [Parámetros de red](#parámetros-de-red)
7. [Parámetros de seguridad](#parámetros-de-seguridad)
8. [Parámetros de rendimiento](#parámetros-de-rendimiento)
9. [Parámetros de memoria](#parámetros-de-memoria)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## ¿Qué es sysctl?

`sysctl` permite leer y modificar parámetros del kernel Linux **en tiempo de ejecución**, sin recompilar ni reiniciar. Estos parámetros controlan el comportamiento de subsistemas del kernel: red, seguridad, memoria, filesystems, scheduling, etc.

```
  ┌──────────────────────────────────────────────────────────────┐
  │                                                              │
  │  Espacio de usuario          │         Kernel                │
  │                              │                               │
  │  sysctl net.ipv4.ip_forward  │                               │
  │       │                      │                               │
  │       ▼                      │                               │
  │  Lee/escribe                 │   ┌──────────────────────┐    │
  │  /proc/sys/net/ipv4/         │   │  Subsistema de red   │    │
  │     ip_forward          ─────┼──▶│  ip_forward = 0|1    │    │
  │                              │   └──────────────────────┘    │
  │                              │                               │
  │  sysctl = interfaz           │   /proc/sys/ = pseudo-        │
  │  de línea de comandos        │   filesystem que expone       │
  │                              │   variables internas          │
  │                              │   del kernel                  │
  └──────────────────────────────────────────────────────────────┘
```

Hay dos formas equivalentes de interactuar con estos parámetros:

```bash
# Method 1: sysctl command
sysctl net.ipv4.ip_forward
# net.ipv4.ip_forward = 0

# Method 2: direct file access
cat /proc/sys/net/ipv4/ip_forward
# 0

# The "dot notation" maps to directory separators:
# net.ipv4.ip_forward → /proc/sys/net/ipv4/ip_forward
```

---

## /proc/sys: la interfaz del kernel

`/proc/sys/` es un pseudo-filesystem — los archivos no existen en disco, son ventanas a variables internas del kernel. Cada archivo representa un parámetro tuneable.

### Estructura de directorios

```
  /proc/sys/
  ├── kernel/              ← Parámetros generales del kernel
  │   ├── hostname
  │   ├── osrelease
  │   ├── panic
  │   ├── pid_max
  │   ├── randomize_va_space   (ASLR)
  │   ├── sysrq
  │   └── ...
  ├── net/                 ← Networking
  │   ├── core/            ← Configuración general de red
  │   │   ├── somaxconn
  │   │   ├── rmem_max
  │   │   └── wmem_max
  │   ├── ipv4/            ← IPv4
  │   │   ├── ip_forward
  │   │   ├── tcp_syncookies
  │   │   ├── conf/        ← Per-interface settings
  │   │   │   ├── all/
  │   │   │   ├── default/
  │   │   │   ├── eth0/
  │   │   │   └── ...
  │   │   └── ...
  │   ├── ipv6/            ← IPv6
  │   └── bridge/          ← Bridging
  ├── vm/                  ← Memoria virtual
  │   ├── swappiness
  │   ├── overcommit_memory
  │   ├── dirty_ratio
  │   └── ...
  ├── fs/                  ← Filesystems
  │   ├── file-max
  │   ├── inotify/
  │   └── ...
  └── dev/                 ← Dispositivos específicos
```

### Tipos de valores

```bash
# Boolean (0 or 1)
cat /proc/sys/net/ipv4/ip_forward
# 0

# Integer
cat /proc/sys/kernel/pid_max
# 4194304

# String
cat /proc/sys/kernel/hostname
# myserver

# Multiple values (space-separated)
cat /proc/sys/net/ipv4/tcp_rmem
# 4096    131072  6291456
# (min    default max)

# Per-interface (directory per interface)
cat /proc/sys/net/ipv4/conf/eth0/rp_filter
# 1
```

---

## Leer parámetros

### Con sysctl

```bash
# Read a specific parameter
sysctl net.ipv4.ip_forward
# net.ipv4.ip_forward = 0

# Read only the value (no name)
sysctl -n net.ipv4.ip_forward
# 0

# Read ALL parameters
sysctl -a
# ... hundreds of lines ...

# Read all parameters matching a pattern
sysctl -a | grep tcp_syn
# net.ipv4.tcp_syn_retries = 6
# net.ipv4.tcp_synack_retries = 5
# net.ipv4.tcp_syncookies = 1

# Read all parameters in a subsystem
sysctl net.ipv4.conf.all
# net.ipv4.conf.all.accept_redirects = 0
# net.ipv4.conf.all.accept_source_route = 0
# ...
```

### Con acceso directo a /proc/sys

```bash
# Equivalent to sysctl
cat /proc/sys/net/ipv4/ip_forward
# 0

# List all parameters in a directory
ls /proc/sys/net/ipv4/

# Find parameters by name
find /proc/sys -name "*syncookies*"
# /proc/sys/net/ipv4/tcp_syncookies
```

---

## Modificar parámetros en caliente

Los cambios con `sysctl -w` o escribiendo en `/proc/sys/` son **inmediatos pero temporales** — se pierden al reiniciar.

### Con sysctl -w

```bash
# Enable IP forwarding (router functionality)
sudo sysctl -w net.ipv4.ip_forward=1
# net.ipv4.ip_forward = 1

# Set multiple parameters at once
sudo sysctl -w net.ipv4.ip_forward=1 \
    net.ipv4.conf.all.rp_filter=1 \
    kernel.panic=10

# Verify
sysctl net.ipv4.ip_forward
# net.ipv4.ip_forward = 1
```

### Con echo (equivalente)

```bash
# Same effect as sysctl -w
echo 1 | sudo tee /proc/sys/net/ipv4/ip_forward

# Or with redirect (need root shell, not just sudo)
sudo sh -c 'echo 1 > /proc/sys/net/ipv4/ip_forward'
```

### Cambios temporales vs persistentes

```
  ┌────────────────────────────────────────────────────────────┐
  │                                                            │
  │  TEMPORAL (se pierde al reiniciar):                        │
  │  sudo sysctl -w net.ipv4.ip_forward=1                     │
  │  echo 1 | sudo tee /proc/sys/net/ipv4/ip_forward          │
  │                                                            │
  │  PERSISTENTE (sobrevive al reiniciar):                     │
  │  Editar /etc/sysctl.d/*.conf                               │
  │  Luego: sudo sysctl --system                               │
  │                                                            │
  └────────────────────────────────────────────────────────────┘
```

---

## Configuración persistente

### Archivos de configuración

```
  ┌────────────────────────────────────────────────────────────┐
  │  ORDEN DE CARGA (menor a mayor prioridad):                 │
  │                                                            │
  │  /usr/lib/sysctl.d/*.conf        ← Distribución           │
  │  /run/sysctl.d/*.conf            ← Runtime (transitorio)  │
  │  /etc/sysctl.d/*.conf            ← Admin (tus cambios)    │
  │  /etc/sysctl.conf                ← Legado (aún funciona)  │
  │                                                            │
  │  Dentro de cada directorio: orden alfabético               │
  │  Si el mismo parámetro aparece en varios archivos,         │
  │  el ÚLTIMO archivo procesado gana                          │
  └────────────────────────────────────────────────────────────┘
```

### Crear configuración personalizada

```bash
# Best practice: create numbered files in /etc/sysctl.d/
sudo tee /etc/sysctl.d/90-custom.conf << 'EOF'
# IP forwarding for routing
net.ipv4.ip_forward = 1

# Security hardening
net.ipv4.conf.all.rp_filter = 1
net.ipv4.conf.all.accept_redirects = 0
net.ipv4.conf.all.send_redirects = 0

# Performance tuning
vm.swappiness = 10
EOF
```

### Aplicar la configuración

```bash
# Apply ALL sysctl config files (in priority order)
sudo sysctl --system
# * Applying /usr/lib/sysctl.d/10-default-yama-scope.conf ...
# * Applying /etc/sysctl.d/90-custom.conf ...
# * Applying /etc/sysctl.conf ...

# Apply a specific file only
sudo sysctl -p /etc/sysctl.d/90-custom.conf

# Apply the legacy /etc/sysctl.conf
sudo sysctl -p
# (without argument, defaults to /etc/sysctl.conf)
```

### Formato del archivo

```bash
# /etc/sysctl.d/90-custom.conf

# Comments start with # or ;
# Empty lines are ignored

# Format: parameter = value
net.ipv4.ip_forward = 1

# Spaces around = are optional
net.ipv4.ip_forward=1    # Also valid

# - (dash) and / are accepted in names and converted to .
# These are equivalent:
# net/ipv4/ip_forward = 1
# net.ipv4.ip_forward = 1

# Glob patterns work for reading but NOT in config files
```

### Archivos de la distribución

```bash
# View distribution defaults
ls /usr/lib/sysctl.d/
# 10-default-yama-scope.conf    ← ptrace scope
# 50-coredump.conf              ← Core dump settings
# 50-default.conf                ← General defaults
# 50-pid-max.conf                ← pid_max

cat /usr/lib/sysctl.d/50-default.conf
# Typical entries:
# net.ipv4.ping_group_range = 0 2147483647
# net.core.default_qdisc = fq_codel
# ...

# NEVER edit files in /usr/lib/sysctl.d/
# Override in /etc/sysctl.d/ instead
```

---

## Parámetros de red

### IP Forwarding

```bash
# Enable IPv4 forwarding (required for routers, NAT, containers)
net.ipv4.ip_forward = 1

# Enable IPv6 forwarding
net.ipv6.conf.all.forwarding = 1

# Per-interface forwarding (override global setting)
net.ipv4.conf.eth0.forwarding = 1
net.ipv4.conf.eth1.forwarding = 0
```

### Protección contra ataques de red

```bash
# ── Reverse Path Filtering (anti-spoofing) ──
# 0 = disabled, 1 = strict (recommended), 2 = loose
net.ipv4.conf.all.rp_filter = 1
net.ipv4.conf.default.rp_filter = 1

# ── SYN Cookies (anti SYN-flood) ──
net.ipv4.tcp_syncookies = 1

# ── ICMP Redirects (prevent routing table manipulation) ──
# Don't accept ICMP redirects
net.ipv4.conf.all.accept_redirects = 0
net.ipv4.conf.default.accept_redirects = 0
net.ipv6.conf.all.accept_redirects = 0

# Don't send ICMP redirects (not a router)
net.ipv4.conf.all.send_redirects = 0
net.ipv4.conf.default.send_redirects = 0

# ── Source Routing (prevent source-routed packets) ──
net.ipv4.conf.all.accept_source_route = 0
net.ipv4.conf.default.accept_source_route = 0
net.ipv6.conf.all.accept_source_route = 0

# ── Log Martian Packets (packets with impossible addresses) ──
net.ipv4.conf.all.log_martians = 1
net.ipv4.conf.default.log_martians = 1

# ── Ignore ICMP broadcasts (anti Smurf attack) ──
net.ipv4.icmp_echo_ignore_broadcasts = 1

# ── Ignore bogus ICMP error responses ──
net.ipv4.icmp_ignore_bogus_error_responses = 1

# ── IPv6 Router Advertisements ──
# Don't accept RA (if this is not a router)
net.ipv6.conf.all.accept_ra = 0
net.ipv6.conf.default.accept_ra = 0
```

### conf/all vs conf/default vs conf/eth0

```
  ┌────────────────────────────────────────────────────────────┐
  │                                                            │
  │  conf/all/          ← Se aplica a TODAS las interfaces     │
  │                       existentes y futuras.                │
  │                       Para algunos parámetros, actúa       │
  │                       como OR: si all=1, todas son 1       │
  │                       sin importar su valor individual.    │
  │                                                            │
  │  conf/default/      ← Valor por defecto para interfaces   │
  │                       NUEVAS (creadas después del cambio). │
  │                       No afecta interfaces existentes.     │
  │                                                            │
  │  conf/<iface>/      ← Valor para UNA interfaz específica. │
  │                                                            │
  │  Recomendación: siempre configura AMBOS all y default      │
  │  para asegurar cobertura completa.                         │
  └────────────────────────────────────────────────────────────┘
```

### TCP Tuning

```bash
# ── Buffer sizes ──
# TCP receive buffer (min, default, max) in bytes
net.ipv4.tcp_rmem = 4096 131072 6291456

# TCP send buffer (min, default, max)
net.ipv4.tcp_wmem = 4096 16384 4194304

# Global max receive/send buffer
net.core.rmem_max = 16777216
net.core.wmem_max = 16777216

# ── Connection handling ──
# Max backlog of pending connections
net.core.somaxconn = 4096

# Max SYN backlog
net.ipv4.tcp_max_syn_backlog = 8192

# Reuse TIME_WAIT sockets for new connections
net.ipv4.tcp_tw_reuse = 1

# TCP keepalive (detect dead connections)
net.ipv4.tcp_keepalive_time = 600      # Seconds before first probe
net.ipv4.tcp_keepalive_intvl = 60      # Seconds between probes
net.ipv4.tcp_keepalive_probes = 5      # Probes before dropping

# ── Congestion control ──
net.ipv4.tcp_congestion_control = bbr   # Modern congestion algorithm
net.core.default_qdisc = fq            # Fair queueing (for BBR)
```

---

## Parámetros de seguridad

```bash
# ── ASLR (Address Space Layout Randomization) ──
# 0 = disabled, 1 = partial, 2 = full (recommended)
kernel.randomize_va_space = 2

# ── Core dumps ──
# Don't dump SUID binaries
fs.suid_dumpable = 0

# ── Kernel pointer restriction ──
# 0 = visible, 1 = hidden for non-root, 2 = hidden for all
kernel.kptr_restrict = 1

# ── dmesg restriction ──
# 0 = all users, 1 = only root (CAP_SYSLOG)
kernel.dmesg_restrict = 1

# ── Magic SysRq ──
# 0 = disabled, 1 = all, bitmap for selective
kernel.sysrq = 0
# In production: disable or restrict to safe operations
# 176 = allow sync(16) + remount-ro(32) + reboot(128)

# ── ptrace scope (Yama LSM) ──
# 0 = no restriction, 1 = only parent can ptrace (recommended)
# 2 = only admin, 3 = no ptrace at all
kernel.yama.ptrace_scope = 1

# ── Symlink/hardlink protection ──
fs.protected_symlinks = 1
fs.protected_hardlinks = 1
fs.protected_fifos = 1
fs.protected_regular = 2

# ── Disable unprivileged BPF ──
kernel.unprivileged_bpf_disabled = 1

# ── Restrict performance events ──
kernel.perf_event_paranoid = 2

# ── Disable user namespaces (if not needed) ──
# user.max_user_namespaces = 0
# Warning: breaks containers and some apps (Chrome, Flatpak)
```

---

## Parámetros de rendimiento

### Scheduling

```bash
# ── Proceso scheduling ──
# Scheduler autogroup (group processes by session for interactive perf)
kernel.sched_autogroup_enabled = 1

# Maximum number of PIDs
kernel.pid_max = 4194304

# Maximum threads system-wide
kernel.threads-max = 256000
```

### Filesystem

```bash
# ── Maximum open files system-wide ──
fs.file-max = 2097152

# ── inotify limits (for IDEs, file watchers) ──
fs.inotify.max_user_watches = 524288
fs.inotify.max_user_instances = 1024
fs.inotify.max_queued_events = 32768

# ── AIO (Async I/O) limits ──
fs.aio-max-nr = 1048576
```

---

## Parámetros de memoria

```bash
# ── Swappiness ──
# 0-100: how aggressively the kernel swaps
# 0 = only swap to avoid OOM, 100 = swap aggressively
# Desktop: 10-30, Server: 10-60 (depends on workload)
vm.swappiness = 10

# ── Overcommit ──
# 0 = heuristic (default), 1 = always allow, 2 = strict (never overcommit)
vm.overcommit_memory = 0
# If set to 2:
vm.overcommit_ratio = 80   # Allow commit up to 80% of RAM + swap

# ── Dirty pages (write-back cache) ──
# Percentage of RAM that can be dirty before background writeback starts
vm.dirty_background_ratio = 5
# Percentage of RAM that can be dirty before processes BLOCK on writes
vm.dirty_ratio = 20
# Alternatives using absolute bytes:
# vm.dirty_background_bytes = 0
# vm.dirty_bytes = 0

# ── OOM Killer ──
# Don't panic on OOM (let OOM killer handle it)
vm.panic_on_oom = 0
# OOM killer selects process to kill based on oom_score
# Per-process: /proc/<pid>/oom_score_adj (-1000 to 1000)

# ── VFS cache pressure ──
# Controls tendency to reclaim dentries/inodes
# 0 = never reclaim, 100 = default, >100 = aggressive
vm.vfs_cache_pressure = 100

# ── Minimum free memory ──
# Pages to keep free (for emergency allocations)
vm.min_free_kbytes = 65536

# ── Huge pages ──
vm.nr_hugepages = 0             # Number of preallocated huge pages
vm.hugetlb_shm_group = 0       # Group allowed to use huge pages
```

### Escenarios de swappiness

```
  ┌────────────────────────────────────────────────────────────┐
  │  VALOR    │  COMPORTAMIENTO          │  CASO DE USO        │
  │───────────┼──────────────────────────┼─────────────────────│
  │  0        │  No swap (casi) a menos  │  Base de datos      │
  │           │  que sea absolutamente    │  (RAM es crucial)   │
  │           │  necesario               │                     │
  │───────────┼──────────────────────────┼─────────────────────│
  │  10       │  Swap mínimo             │  Desktop, laptop    │
  │           │                          │  Servidor general   │
  │───────────┼──────────────────────────┼─────────────────────│
  │  60       │  Default del kernel      │  Balance general    │
  │           │                          │                     │
  │───────────┼──────────────────────────┼─────────────────────│
  │  100      │  Swap agresivo           │  Sistemas con       │
  │           │  (prefiere swap sobre    │  mucha swap y       │
  │           │   soltar page cache)     │  poca RAM           │
  └────────────────────────────────────────────────────────────┘
```

---

## Errores comunes

### 1. Editar /proc/sys con un editor de texto

```bash
# WRONG: editors create temp files, rename, etc — doesn't work on /proc
sudo vim /proc/sys/net/ipv4/ip_forward
# This will NOT work as expected

# RIGHT: use echo/tee or sysctl -w
echo 1 | sudo tee /proc/sys/net/ipv4/ip_forward
sudo sysctl -w net.ipv4.ip_forward=1
```

### 2. Olvidar que sysctl -w es temporal

```bash
# This works NOW but is lost on reboot:
sudo sysctl -w vm.swappiness=10

# For persistence, also add to /etc/sysctl.d/:
echo "vm.swappiness = 10" | sudo tee /etc/sysctl.d/90-custom.conf
sudo sysctl --system
```

### 3. Configurar conf/default creyendo que afecta interfaces existentes

```bash
# This only affects FUTURE interfaces, not existing ones:
sudo sysctl -w net.ipv4.conf.default.rp_filter=1

# For existing interfaces, you need conf/all:
sudo sysctl -w net.ipv4.conf.all.rp_filter=1

# Best: set BOTH
net.ipv4.conf.all.rp_filter = 1
net.ipv4.conf.default.rp_filter = 1
```

### 4. No usar sysctl --system al cambiar archivos

```bash
# After creating/editing /etc/sysctl.d/90-custom.conf:

# INCOMPLETE: only loads /etc/sysctl.conf
sudo sysctl -p

# COMPLETE: loads ALL config files in correct order
sudo sysctl --system
```

### 5. Habilitar ip_forward sin entender las implicaciones

```bash
# ip_forward=1 makes the machine a ROUTER
# It will forward packets between interfaces
# Combined with no firewall = open relay

# Always pair with firewall rules:
net.ipv4.ip_forward = 1
# + iptables/nftables rules to control what gets forwarded
```

---

## Cheatsheet

```bash
# ═══════════════════════════════════════════════
#  LEER
# ═══════════════════════════════════════════════
sysctl <param>                       # Read (with name)
sysctl -n <param>                    # Read (value only)
sysctl -a                            # Read ALL
sysctl -a | grep <pattern>           # Search
cat /proc/sys/path/to/param          # Direct read

# ═══════════════════════════════════════════════
#  MODIFICAR (temporal)
# ═══════════════════════════════════════════════
sudo sysctl -w <param>=<value>       # Set one
sudo sysctl -w p1=v1 p2=v2          # Set multiple
echo <value> | sudo tee /proc/sys/path/to/param

# ═══════════════════════════════════════════════
#  PERSISTENTE
# ═══════════════════════════════════════════════
# Create/edit:
sudo vim /etc/sysctl.d/90-custom.conf
# Apply all:
sudo sysctl --system
# Apply one file:
sudo sysctl -p /etc/sysctl.d/90-custom.conf
# Apply legacy /etc/sysctl.conf:
sudo sysctl -p

# ═══════════════════════════════════════════════
#  SEGURIDAD (hardening)
# ═══════════════════════════════════════════════
kernel.randomize_va_space = 2        # Full ASLR
kernel.kptr_restrict = 1             # Hide kernel pointers
kernel.dmesg_restrict = 1            # Root-only dmesg
kernel.sysrq = 0                     # Disable SysRq
kernel.yama.ptrace_scope = 1         # Restrict ptrace
fs.suid_dumpable = 0                 # No SUID core dumps
fs.protected_symlinks = 1            # Symlink protection
fs.protected_hardlinks = 1           # Hardlink protection

# ═══════════════════════════════════════════════
#  RED (seguridad)
# ═══════════════════════════════════════════════
net.ipv4.conf.all.rp_filter = 1             # Anti-spoofing
net.ipv4.tcp_syncookies = 1                  # Anti SYN-flood
net.ipv4.conf.all.accept_redirects = 0       # No ICMP redirect
net.ipv4.conf.all.send_redirects = 0         # Don't send redirect
net.ipv4.conf.all.accept_source_route = 0    # No source routing
net.ipv4.conf.all.log_martians = 1           # Log spoofed packets
net.ipv4.icmp_echo_ignore_broadcasts = 1     # Anti Smurf

# ═══════════════════════════════════════════════
#  RED (rendimiento)
# ═══════════════════════════════════════════════
net.core.somaxconn = 4096                    # Backlog queue
net.ipv4.tcp_tw_reuse = 1                    # Reuse TIME_WAIT
net.ipv4.tcp_congestion_control = bbr        # BBR congestion
net.core.default_qdisc = fq                  # Fair queueing

# ═══════════════════════════════════════════════
#  MEMORIA
# ═══════════════════════════════════════════════
vm.swappiness = 10                           # Less swapping
vm.dirty_ratio = 20                          # Write-back threshold
vm.dirty_background_ratio = 5                # Background writeback
vm.overcommit_memory = 0                     # Heuristic overcommit

# ═══════════════════════════════════════════════
#  FILESYSTEM
# ═══════════════════════════════════════════════
fs.file-max = 2097152                        # Max open files
fs.inotify.max_user_watches = 524288         # For IDEs/watchers
```

---

## Ejercicios

### Ejercicio 1: Auditar la configuración actual

```bash
# Step 1: check key security parameters
echo "=== Security Parameters ==="
for param in \
    kernel.randomize_va_space \
    kernel.kptr_restrict \
    kernel.dmesg_restrict \
    kernel.sysrq \
    kernel.yama.ptrace_scope \
    fs.suid_dumpable \
    fs.protected_symlinks \
    fs.protected_hardlinks; do
    echo "  $param = $(sysctl -n $param 2>/dev/null || echo 'N/A')"
done

# Step 2: check key network security parameters
echo "=== Network Security ==="
for param in \
    net.ipv4.ip_forward \
    net.ipv4.conf.all.rp_filter \
    net.ipv4.tcp_syncookies \
    net.ipv4.conf.all.accept_redirects \
    net.ipv4.conf.all.accept_source_route \
    net.ipv4.conf.all.log_martians \
    net.ipv4.icmp_echo_ignore_broadcasts; do
    echo "  $param = $(sysctl -n $param 2>/dev/null || echo 'N/A')"
done

# Step 3: check memory parameters
echo "=== Memory ==="
for param in \
    vm.swappiness \
    vm.overcommit_memory \
    vm.dirty_ratio \
    vm.dirty_background_ratio; do
    echo "  $param = $(sysctl -n $param)"
done

# Step 4: list all custom sysctl files
echo "=== Custom sysctl files ==="
ls /etc/sysctl.d/*.conf 2>/dev/null
cat /etc/sysctl.conf 2>/dev/null | grep -v "^#" | grep -v "^$"

# Step 5: count total tunable parameters
echo "=== Total parameters ==="
sysctl -a 2>/dev/null | wc -l
```

**Preguntas:**
1. ¿ASLR está habilitado completamente (valor 2)?
2. ¿IP forwarding está habilitado? ¿Debería estarlo en tu sistema?
3. ¿Cuál es el valor de `swappiness`? ¿Es el default o fue modificado?

> **Pregunta de predicción**: si `net.ipv4.conf.all.accept_redirects = 0` pero `net.ipv4.conf.eth0.accept_redirects = 1`, ¿la interfaz `eth0` aceptará ICMP redirects? ¿Cuál de los dos valores prevalece?

### Ejercicio 2: Modificar parámetros de forma temporal y persistente

```bash
# Step 1: record original swappiness
ORIGINAL=$(sysctl -n vm.swappiness)
echo "Original swappiness: $ORIGINAL"

# Step 2: change temporarily
sudo sysctl -w vm.swappiness=10
sysctl -n vm.swappiness
# Should show 10

# Step 3: verify via /proc
cat /proc/sys/vm/swappiness
# Should also show 10

# Step 4: restore original value
sudo sysctl -w vm.swappiness=$ORIGINAL

# Step 5: now make a PERSISTENT change
sudo tee /etc/sysctl.d/99-lab-test.conf << 'EOF'
# Lab exercise — test persistent sysctl
vm.swappiness = 10
fs.inotify.max_user_watches = 524288
EOF

# Step 6: apply without reboot
sudo sysctl --system 2>&1 | grep "99-lab"

# Step 7: verify
sysctl vm.swappiness
sysctl fs.inotify.max_user_watches

# Step 8: clean up
sudo rm /etc/sysctl.d/99-lab-test.conf
sudo sysctl -w vm.swappiness=$ORIGINAL
sudo sysctl --system
```

**Preguntas:**
1. ¿Cuál es la diferencia entre `sysctl -p` y `sysctl --system`?
2. ¿El cambio temporal (step 2) se reflejó inmediatamente en `/proc/sys/`?
3. Si reinicias sin eliminar `99-lab-test.conf`, ¿qué valor tendrá swappiness?

> **Pregunta de predicción**: si creas `/etc/sysctl.d/50-server.conf` con `vm.swappiness = 30` y `/etc/sysctl.d/90-custom.conf` con `vm.swappiness = 10`, ¿qué valor tendrá swappiness después de `sysctl --system`? ¿Y si además `/etc/sysctl.conf` tiene `vm.swappiness = 60`?

### Ejercicio 3: Crear un perfil de hardening

```bash
# Step 1: create a comprehensive hardening sysctl file
sudo tee /etc/sysctl.d/80-hardening.conf << 'EOF'
# === Kernel security ===
kernel.randomize_va_space = 2
kernel.kptr_restrict = 1
kernel.dmesg_restrict = 1
kernel.sysrq = 0
fs.suid_dumpable = 0
fs.protected_symlinks = 1
fs.protected_hardlinks = 1

# === Network security ===
net.ipv4.conf.all.rp_filter = 1
net.ipv4.conf.default.rp_filter = 1
net.ipv4.tcp_syncookies = 1
net.ipv4.conf.all.accept_redirects = 0
net.ipv4.conf.default.accept_redirects = 0
net.ipv4.conf.all.send_redirects = 0
net.ipv4.conf.default.send_redirects = 0
net.ipv4.conf.all.accept_source_route = 0
net.ipv4.conf.default.accept_source_route = 0
net.ipv4.conf.all.log_martians = 1
net.ipv4.icmp_echo_ignore_broadcasts = 1
net.ipv6.conf.all.accept_redirects = 0
net.ipv6.conf.default.accept_redirects = 0
net.ipv6.conf.all.accept_source_route = 0
EOF

# Step 2: check for syntax errors (dry run)
sudo sysctl -p /etc/sysctl.d/80-hardening.conf

# Step 3: verify each parameter was applied
grep -v "^#" /etc/sysctl.d/80-hardening.conf | grep -v "^$" | \
while IFS='=' read -r param value; do
    param=$(echo $param | xargs)   # trim whitespace
    actual=$(sysctl -n $param 2>/dev/null)
    expected=$(echo $value | xargs)
    if [ "$actual" = "$expected" ]; then
        echo "OK: $param = $actual"
    else
        echo "MISMATCH: $param expected=$expected actual=$actual"
    fi
done

# Step 4: compare with a non-hardened default
echo "=== Before vs After comparison ==="
echo "dmesg_restrict: $(sysctl -n kernel.dmesg_restrict)"
echo "sysrq: $(sysctl -n kernel.sysrq)"
echo "rp_filter: $(sysctl -n net.ipv4.conf.all.rp_filter)"

# Step 5: clean up (remove if this is just a lab)
sudo rm /etc/sysctl.d/80-hardening.conf
sudo sysctl --system
```

**Preguntas:**
1. ¿Algún parámetro mostró MISMATCH? ¿Cuál y por qué?
2. ¿Deshabilitar SysRq (`kernel.sysrq = 0`) es siempre recomendable? ¿Cuándo querrías mantenerlo?
3. ¿Este perfil de hardening afectaría la funcionalidad normal de un servidor web o SSH?

> **Pregunta de predicción**: si aplicas este perfil de hardening y luego necesitas que el servidor actúe como router (forward de paquetes), ¿qué parámetro falta? ¿Tendrías que modificar también los parámetros de `send_redirects` y `accept_redirects`?

---

> **Siguiente tema**: T02 — /proc y /sys en profundidad: información del kernel, tuning de red, scheduler.
