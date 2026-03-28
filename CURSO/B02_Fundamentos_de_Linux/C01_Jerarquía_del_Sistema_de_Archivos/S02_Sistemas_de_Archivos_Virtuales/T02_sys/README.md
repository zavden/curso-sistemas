# T02 — /sys

## Qué es /sys

`/sys` (sysfs) es un filesystem virtual que expone el **modelo de dispositivos
del kernel** de forma jerárquica. Fue introducido en Linux 2.6 (2004) para
reemplazar la información de dispositivos que antes se obtenía de `/proc`.

```bash
mount | grep sysfs
# sysfs on /sys type sysfs (rw,nosuid,nodev,noexec,relatime)
```

Mientras `/proc` mezcla información de procesos con dispositivos y parámetros
del kernel, `/sys` organiza exclusivamente la información de hardware y
subsistemas del kernel en una jerarquía clara.

```
/proc → procesos + información variada del sistema (herencia histórica)
/sys  → dispositivos y subsistemas del kernel (diseño moderno y limpio)
```

## Estructura principal

```
/sys/
├── block/      ← Dispositivos de bloque (discos)
├── bus/        ← Buses del sistema (pci, usb, scsi, i2c)
├── class/      ← Dispositivos agrupados por función (net, block, tty, input)
├── devices/    ← Jerarquía física real de dispositivos
├── firmware/   ← Interfaces de firmware (ACPI, EFI, DMI)
├── fs/         ← Parámetros de filesystems
├── kernel/     ← Parámetros y estado del kernel
├── module/     ← Módulos del kernel cargados
└── power/      ← Gestión de energía
```

## /sys/block — Dispositivos de bloque

Lista todos los dispositivos de bloque (discos) del sistema:

```bash
ls /sys/block/
# sda  sdb  loop0  loop1  dm-0  sr0

# Tamaño de un disco (en sectores de 512 bytes)
cat /sys/block/sda/size
# 976773168

# Convertir a GB
echo $(($(cat /sys/block/sda/size) * 512 / 1024 / 1024 / 1024))
# 465

# Scheduler de I/O activo
cat /sys/block/sda/queue/scheduler
# [mq-deadline] kyber bfq none

# Particiones del disco
ls /sys/block/sda/sda*/
# sda1  sda2  sda3

# Tamaño de una partición
cat /sys/block/sda/sda1/size
```

En discos NVMe, la nomenclatura es diferente:

```bash
ls /sys/block/
# nvme0n1

ls /sys/block/nvme0n1/nvme0n1p*/
# nvme0n1p1  nvme0n1p2
```

## /sys/bus — Buses del sistema

Organiza los dispositivos por el bus al que están conectados:

```bash
ls /sys/bus/
# acpi  cpu  i2c  pci  scsi  usb  virtio  ...

# Dispositivos PCI
ls /sys/bus/pci/devices/
# 0000:00:00.0  0000:00:01.0  0000:00:1f.0  ...

# Dispositivos USB
ls /sys/bus/usb/devices/
# 1-0:1.0  1-1  1-1:1.0  2-0:1.0  usb1  usb2
```

Cada entrada es un symlink al dispositivo real en `/sys/devices/`.

## /sys/class — Dispositivos por función

Agrupa dispositivos por su **tipo funcional**, independientemente de cómo están
conectados físicamente:

```bash
ls /sys/class/
# block  dmi  hwmon  input  leds  mem  misc  net  pci_bus
# power_supply  rtc  scsi_host  thermal  tty  ...
```

### Interfaces de red

```bash
ls /sys/class/net/
# eth0  lo  docker0  veth1234

# Dirección MAC
cat /sys/class/net/eth0/address
# aa:bb:cc:dd:ee:ff

# Estado del enlace (up/down)
cat /sys/class/net/eth0/operstate
# up

# Velocidad (Mbps, si es enlace físico)
cat /sys/class/net/eth0/speed
# 1000

# MTU
cat /sys/class/net/eth0/mtu
# 1500

# Estadísticas de red
cat /sys/class/net/eth0/statistics/rx_bytes
cat /sys/class/net/eth0/statistics/tx_bytes
```

### Dispositivos de bloque

```bash
ls /sys/class/block/
# sda  sda1  sda2  loop0  dm-0

# Es un atajo a /sys/block/ (con más detalle en algunos casos)
```

### Terminales

```bash
ls /sys/class/tty/
# console  tty  tty0  tty1  ttyS0  ttyS1  pts  ...

# Ver el tipo de una terminal serial
cat /sys/class/tty/ttyS0/type
# 4
```

### Sensores de temperatura

```bash
ls /sys/class/thermal/
# thermal_zone0  thermal_zone1  cooling_device0

# Temperatura actual (en milligrados Celsius)
cat /sys/class/thermal/thermal_zone0/temp
# 45000  (= 45.0°C)

# Tipo de zona térmica
cat /sys/class/thermal/thermal_zone0/type
# x86_pkg_temp
```

### Fuentes de alimentación

```bash
ls /sys/class/power_supply/
# AC0  BAT0  (en laptops)

# Estado de la batería
cat /sys/class/power_supply/BAT0/status
# Charging | Discharging | Full

# Capacidad (porcentaje)
cat /sys/class/power_supply/BAT0/capacity
# 75
```

## /sys/devices — Jerarquía física

Es la representación real de cómo los dispositivos están conectados
físicamente. Todos los symlinks en `/sys/class/` y `/sys/bus/` apuntan aquí:

```bash
# Ver a dónde apunta un dispositivo de clase
readlink /sys/class/net/eth0
# ../../devices/pci0000:00/0000:00:03.0/net/eth0

# La jerarquía refleja la conexión física:
# plataforma → bus PCI → dispositivo PCI → interfaz de red
```

Esta jerarquía es la que usa udev para construir las reglas de nombrado y los
symlinks en `/dev/`.

## /sys/module — Módulos del kernel

Información sobre cada módulo (driver) cargado en el kernel:

```bash
ls /sys/module/ | head -10
# acpi  bridge  dm_mod  ext4  ip_tables  kvm  loop  nfs  ...

# Parámetros de un módulo
ls /sys/module/loop/parameters/
# max_loop  max_part

cat /sys/module/loop/parameters/max_loop
# 0  (ilimitado en kernels modernos)

# Versión de un módulo (si la expone)
cat /sys/module/ext4/version 2>/dev/null
```

Comparar con `lsmod` que muestra los módulos cargados y sus dependencias:

```bash
lsmod | head -5
# Module                  Size  Used by
# ext4                  811008  1
# jbd2                  131072  1 ext4
# ...
```

## /sys/fs — Parámetros de filesystems

```bash
ls /sys/fs/
# bpf  cgroup  ext4  fuse  pstore  selinux

# Parámetros de ext4 por dispositivo montado
ls /sys/fs/ext4/
# sda2  sdb1

cat /sys/fs/ext4/sda2/session_write_kbytes
# 123456

# cgroups (usado por Docker y systemd)
ls /sys/fs/cgroup/
# cpu  memory  blkio  ...  (cgroups v1)
# o un árbol unificado         (cgroups v2)
```

## /sys/kernel — Estado del kernel

```bash
ls /sys/kernel/
# debug  kexec_loaded  mm  notes  profiling  security  uevent_helper

# Información de debug (debugfs montado en /sys/kernel/debug)
ls /sys/kernel/debug/ 2>/dev/null
# block  dma_buf  extfrag  gpio  ...
```

## /sys/firmware — Firmware y BIOS

```bash
ls /sys/firmware/
# acpi  dmi  efi  memmap

# Información del fabricante (DMI/SMBIOS)
cat /sys/firmware/dmi/tables/DMI 2>/dev/null
# (binario — usar dmidecode para leerlo)

# Equivalente legible:
sudo dmidecode -t system | head -10
# System Information
#   Manufacturer: Dell Inc.
#   Product Name: PowerEdge R640
#   ...
```

## /sys es escribible (parcialmente)

Al igual que `/proc/sys`, algunos archivos en `/sys` son escribibles para
modificar el comportamiento de dispositivos:

```bash
# Cambiar el scheduler de I/O de un disco
echo mq-deadline > /sys/block/sda/queue/scheduler

# Cambiar el brillo de la pantalla (laptops)
echo 50 > /sys/class/backlight/intel_backlight/brightness

# Desactivar un dispositivo USB
echo 0 > /sys/bus/usb/devices/1-1/authorized
```

Estos cambios son **temporales** — se pierden al reiniciar. Para hacerlos
permanentes, se usan reglas de udev o scripts de systemd.

## /proc vs /sys para dispositivos

Históricamente, `/proc` contenía información de dispositivos porque sysfs no
existía. Muchos archivos migaron a `/sys`, pero por compatibilidad, algunos
se mantienen en ambos:

| Información | /proc | /sys |
|---|---|---|
| Particiones | `/proc/partitions` | `/sys/block/*/` |
| Interfaces de red | `/proc/net/dev` | `/sys/class/net/*/statistics/` |
| IRQs | `/proc/interrupts` | `/sys/kernel/irq/` |
| Módulos | `/proc/modules` | `/sys/module/` |
| CPU | `/proc/cpuinfo` | `/sys/devices/system/cpu/` |

La tendencia es que la información nueva de dispositivos solo se expone en
`/sys`, mientras `/proc` mantiene las interfaces legacy por compatibilidad.

---

## Ejercicios

### Ejercicio 1 — Explorar dispositivos de bloque

```bash
# Listar discos
ls /sys/block/

# Ver el tamaño y scheduler de cada disco
for disk in /sys/block/sd* /sys/block/nvme* /sys/block/vd* 2>/dev/null; do
    [ -d "$disk" ] || continue
    name=$(basename "$disk")
    size=$(($(cat "$disk/size") * 512 / 1024 / 1024 / 1024))
    sched=$(cat "$disk/queue/scheduler" 2>/dev/null)
    echo "$name: ${size}GB, scheduler: $sched"
done
```

### Ejercicio 2 — Interfaces de red

```bash
# Listar interfaces con su estado
for iface in /sys/class/net/*; do
    name=$(basename "$iface")
    state=$(cat "$iface/operstate" 2>/dev/null)
    mac=$(cat "$iface/address" 2>/dev/null)
    echo "$name: state=$state mac=$mac"
done
```

### Ejercicio 3 — Seguir los symlinks

```bash
# Ver cómo /sys/class apunta a /sys/devices
readlink -f /sys/class/net/lo
readlink -f /sys/class/block/sda 2>/dev/null

# Comparar la vista por clase vs la vista física
```
