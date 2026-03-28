# T02 — /sys

## Objetivo

Explorar el filesystem virtual `/sys` (sysfs): estructura de categorías,
información de dispositivos de bloque y red, jerarquía de symlinks entre vista
lógica y física, y la relación con `/proc`.

---

## Notas respecto al material original

1. **`ip` no lee de `/sys/`** — El lab original dice que `ip` "obtiene los
   datos de /sys internamente". Esto es incorrecto. `ip` usa **netlink sockets**
   (`AF_NETLINK`) para comunicarse directamente con el kernel. Tanto sysfs como
   netlink obtienen los mismos datos del kernel, pero son interfaces
   independientes. `ip` no lee archivos de `/sys/class/net/`.

---

## Qué es /sys

`/sys` (sysfs) es un filesystem virtual que expone el **modelo de dispositivos
del kernel** de forma jerárquica. Fue introducido en Linux 2.6 (2003) para
organizar la información de dispositivos que antes se obtenía mezclada en
`/proc`.

```bash
mount | grep sysfs
# sysfs on /sys type sysfs (rw,nosuid,nodev,noexec,relatime)
```

```
/proc → procesos + información variada del sistema (herencia histórica)
/sys  → dispositivos y subsistemas del kernel (diseño moderno y limpio)
```

## Estructura principal

```
/sys/
├── block/      ← Dispositivos de bloque (discos)
├── bus/        ← Buses del sistema (pci, usb, scsi, virtio)
├── class/      ← Dispositivos agrupados por función (net, block, tty)
├── devices/    ← Jerarquía física real de dispositivos
├── firmware/   ← Interfaces de firmware (ACPI, EFI, DMI)
├── fs/         ← Parámetros de filesystems (cgroup, ext4)
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

# Dispositivos USB
ls /sys/bus/usb/devices/
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

# Estado del enlace (up/down)
cat /sys/class/net/eth0/operstate

# MTU
cat /sys/class/net/eth0/mtu
# 1500

# Estadísticas de red
cat /sys/class/net/eth0/statistics/rx_bytes
cat /sys/class/net/eth0/statistics/tx_bytes
```

### Sensores de temperatura

```bash
ls /sys/class/thermal/
# thermal_zone0  thermal_zone1  cooling_device0

# Temperatura actual (en miligrados Celsius)
cat /sys/class/thermal/thermal_zone0/temp
# 45000  (= 45.0°C)

# Tipo de zona térmica
cat /sys/class/thermal/thermal_zone0/type
# x86_pkg_temp
```

### Fuentes de alimentación (laptops)

```bash
ls /sys/class/power_supply/
# AC0  BAT0

cat /sys/class/power_supply/BAT0/status
# Charging | Discharging | Full

cat /sys/class/power_supply/BAT0/capacity
# 75  (porcentaje)
```

### Terminales

```bash
ls /sys/class/tty/
# console  tty  tty0  ttyS0  pts  ...
```

## /sys/devices — Jerarquía física

Es la representación real de cómo los dispositivos están conectados
físicamente. Todos los symlinks en `/sys/class/` y `/sys/bus/` apuntan aquí:

```bash
# /sys/class/net/eth0 es un symlink
readlink /sys/class/net/eth0
# ../../devices/pci0000:00/0000:00:03.0/net/eth0

# La jerarquía refleja la conexión física:
# plataforma → bus PCI → dispositivo PCI → interfaz de red
```

Los dispositivos virtuales (sin hardware real) están en `/sys/devices/virtual/`:

```bash
readlink -f /sys/class/net/lo
# /sys/devices/virtual/net/lo
```

## /sys/module — Módulos del kernel

Información sobre cada módulo (driver) cargado:

```bash
ls /sys/module/ | head -10

# Parámetros de un módulo
ls /sys/module/loop/parameters/
# max_loop  max_part

cat /sys/module/loop/parameters/max_loop
# 0  (ilimitado en kernels modernos)
```

Comparar con `lsmod` que muestra módulos y sus dependencias:

```bash
lsmod | head -5
# Module                  Size  Used by
# ext4                  811008  1
# jbd2                  131072  1 ext4
```

## /sys/fs — Parámetros de filesystems

```bash
ls /sys/fs/
# bpf  cgroup  ext4  fuse  pstore

# cgroups (usado por Docker y systemd para limitar recursos)
ls /sys/fs/cgroup/
```

## /sys es escribible (parcialmente)

Al igual que `/proc/sys`, algunos archivos en `/sys` son escribibles:

```bash
# Cambiar el scheduler de I/O
echo mq-deadline > /sys/block/sda/queue/scheduler

# Cambiar brillo de pantalla (laptops)
echo 50 > /sys/class/backlight/intel_backlight/brightness
```

Estos cambios son **temporales** — se pierden al reiniciar. Para hacerlos
permanentes, se usan reglas de udev o scripts de systemd.

## /proc vs /sys para dispositivos

| Información | /proc (legacy) | /sys (moderno) |
|---|---|---|
| Particiones | `/proc/partitions` | `/sys/block/*/` |
| Interfaces de red | `/proc/net/dev` | `/sys/class/net/*/statistics/` |
| IRQs | `/proc/interrupts` | `/sys/kernel/irq/` |
| Módulos | `/proc/modules` | `/sys/module/` |
| CPU | `/proc/cpuinfo` | `/sys/devices/system/cpu/` |

La información nueva de dispositivos solo se expone en `/sys`. `/proc` mantiene
las interfaces legacy por compatibilidad.

---

## Ejercicios

Todos los comandos se ejecutan dentro de los contenedores del curso.

### Ejercicio 1 — Explorar la estructura de /sys

Examina las categorías principales y entiende la organización.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Categorías de /sys ==="
ls /sys/

echo ""
echo "=== Tipo de filesystem ==="
mount | grep sysfs

echo ""
echo "=== Clases de dispositivos ==="
ls /sys/class/
echo ""
echo "Total: $(ls /sys/class/ | wc -l) clases"

echo ""
echo "=== Buses ==="
ls /sys/bus/ 2>/dev/null
'
```

<details>
<summary>Predicción</summary>

`/sys` muestra las categorías principales: `block/`, `bus/`, `class/`,
`devices/`, `firmware/`, `fs/`, `kernel/`, `module/`, `power/`.

`mount | grep sysfs` confirma que es un filesystem virtual de tipo `sysfs`
montado en `/sys` con flags `rw,nosuid,nodev,noexec,relatime`.

`/sys/class/` tiene decenas de clases: `net/`, `block/`, `tty/`, `mem/`,
`misc/`, etc. Cada clase agrupa dispositivos por función.

Dentro de un contenedor, la visibilidad de `/sys/bus/` puede ser limitada
(algunos buses del host no son accesibles por el contenedor dependiendo de
la configuración de seguridad). Los buses comunes que sí aparecen: `cpu`,
`container` (si existe), `virtio`.

</details>

---

### Ejercicio 2 — Inspeccionar interfaces de red

Lee información de red directamente desde `/sys/class/net/`.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Interfaces de red ==="
ls /sys/class/net/

echo ""
echo "=== Detalle de cada interfaz ==="
for iface in /sys/class/net/*; do
    name=$(basename "$iface")
    state=$(cat "$iface/operstate" 2>/dev/null || echo "unknown")
    mac=$(cat "$iface/address" 2>/dev/null || echo "n/a")
    mtu=$(cat "$iface/mtu" 2>/dev/null || echo "n/a")
    echo "$name: state=$state mac=$mac mtu=$mtu"
done
'
```

<details>
<summary>Predicción</summary>

El contenedor tiene al menos dos interfaces:
- `lo` (loopback): state=`unknown` (loopback no tiene "up/down" convencional),
  mac=`00:00:00:00:00:00`, mtu=`65536`
- `eth0` (o similar): state=`up`, mac=dirección MAC asignada por Docker,
  mtu=`1500`

`operstate` refleja el estado del enlace a nivel de capa 2:
- `up` — enlace activo
- `down` — enlace inactivo
- `unknown` — no aplicable (loopback, veth)

La MTU de loopback es 65536 (mucho mayor que eth0) porque no pasa por ningún
medio físico y puede usar paquetes enormes. La MTU de eth0 es 1500 (estándar
Ethernet).

El script itera sobre cada interfaz leyendo archivos individuales en
`/sys/class/net/<nombre>/`. Cada atributo es un archivo separado — esto es el
diseño de sysfs: un valor por archivo, fácil de leer desde scripts.

</details>

---

### Ejercicio 3 — Estadísticas de red en tiempo real

Lee contadores de tráfico y observa cómo cambian.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Estadísticas de eth0 ==="
echo "Bytes recibidos: $(cat /sys/class/net/eth0/statistics/rx_bytes)"
echo "Bytes enviados:  $(cat /sys/class/net/eth0/statistics/tx_bytes)"
echo "Paquetes RX:     $(cat /sys/class/net/eth0/statistics/rx_packets)"
echo "Paquetes TX:     $(cat /sys/class/net/eth0/statistics/tx_packets)"
echo "Errores RX:      $(cat /sys/class/net/eth0/statistics/rx_errors)"
echo "Errores TX:      $(cat /sys/class/net/eth0/statistics/tx_errors)"
echo "Drops RX:        $(cat /sys/class/net/eth0/statistics/rx_dropped)"

echo ""
echo "=== Generar tráfico y medir de nuevo ==="
rx_before=$(cat /sys/class/net/eth0/statistics/rx_bytes)
ping -c 3 localhost > /dev/null 2>&1
rx_after=$(cat /sys/class/net/eth0/statistics/rx_bytes)
echo "Bytes RX antes: $rx_before"
echo "Bytes RX después: $rx_after"
echo "Diferencia: $((rx_after - rx_before)) bytes"
'
```

<details>
<summary>Predicción</summary>

Los contadores son acumulativos desde que se creó la interfaz. `rx_bytes` y
`tx_bytes` muestran el tráfico total recibido y enviado.

El `ping -c 3 localhost` genera tráfico en la interfaz **loopback** (`lo`),
no en `eth0`. Así que los contadores de `eth0` probablemente no cambien con
el ping a localhost.

Para ver cambio en `eth0`, se necesitaría tráfico externo (por ejemplo,
`ping -c 3 alma-dev` que usaría la red de Docker). La diferencia de `rx_bytes`
sería pequeña (unos cientos de bytes por los paquetes ICMP y la resolución DNS).

Los errores y drops deberían ser 0 en una red Docker virtual (sin problemas
de medio físico). En interfaces reales, errores > 0 indican problemas de
hardware o configuración.

</details>

---

### Ejercicio 4 — Seguir los symlinks de /sys/class a /sys/devices

Entiende cómo la vista lógica conecta con la jerarquía física.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== /sys/class/net/eth0 ==="
echo "Symlink directo:"
readlink /sys/class/net/eth0

echo ""
echo "Ruta real (todos los symlinks resueltos):"
readlink -f /sys/class/net/eth0

echo ""
echo "=== /sys/class/net/lo ==="
echo "Symlink directo:"
readlink /sys/class/net/lo

echo ""
echo "Ruta real:"
readlink -f /sys/class/net/lo

echo ""
echo "=== Interpretación ==="
echo "eth0 está en /sys/devices/... (hardware virtual)"
echo "lo está en /sys/devices/virtual/ (sin hardware)"
'
```

<details>
<summary>Predicción</summary>

`readlink` (sin `-f`) muestra el symlink tal cual está almacenado, con rutas
relativas como `../../devices/virtual/net/lo`.

`readlink -f` resuelve toda la cadena de symlinks y devuelve la ruta absoluta
final en `/sys/devices/`.

Para **lo** (loopback): la ruta real es `/sys/devices/virtual/net/lo`. El
directorio `virtual/` agrupa dispositivos que no tienen hardware físico
asociado.

Para **eth0**: la ruta real depende del tipo de virtualización. En Docker con
veth pairs, puede estar en `/sys/devices/virtual/net/eth0` (ya que la interfaz
del contenedor es un veth virtual). En una VM con dispositivo PCI emulado,
estaría en algo como `/sys/devices/pci0000:00/.../net/eth0`.

La jerarquía de `/sys/devices/` refleja la topología real:
- `virtual/` → sin hardware
- `pci0000:00/0000:00:XX.Y/` → dispositivo en bus PCI
- `platform/` → dispositivos de plataforma (integrados en el SoC)

</details>

---

### Ejercicio 5 — Explorar dispositivos de bloque

Examina los discos visibles desde el contenedor.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Dispositivos de bloque ==="
ls /sys/block/ 2>/dev/null

echo ""
echo "=== Detalle de cada dispositivo ==="
for disk in /sys/block/*; do
    name=$(basename "$disk")
    size_sectors=$(cat "$disk/size" 2>/dev/null || echo "0")
    size_gb=$((size_sectors * 512 / 1024 / 1024 / 1024))
    sched=$(cat "$disk/queue/scheduler" 2>/dev/null || echo "n/a")
    ro=$(cat "$disk/ro" 2>/dev/null || echo "?")
    echo "$name: ${size_gb}GB  scheduler=$sched  readonly=$ro"
done
'
```

<details>
<summary>Predicción</summary>

Los dispositivos visibles dependen de la configuración del contenedor. En un
contenedor Docker estándar, se ven los dispositivos de bloque del host (aunque
el acceso a ellos está restringido). Pueden incluir:

- `sda` (o `nvme0n1`) — disco principal del host
- `loop0`, `loop1`, ... — dispositivos loop (usados por snap, Docker overlay)
- `dm-0`, `dm-1` — device mapper (LVM, Docker devicemapper)

El tamaño se calcula a partir de `size` (en sectores de 512 bytes). Un disco
de 500 GB muestra ~976 millones de sectores.

`scheduler` muestra el algoritmo de I/O scheduling:
- `[mq-deadline]` — default en muchos kernels, bueno para SSDs
- `none` — para NVMe (no necesitan scheduling porque son rápidos)
- `bfq` — Budget Fair Queueing, bueno para escritorio

`ro` = 0 significa read-write, 1 = read-only.

En contenedores, el acceso directo a los dispositivos de bloque está
restringido por los device cgroups.

</details>

---

### Ejercicio 6 — Módulos del kernel

Examina los módulos cargados y sus parámetros.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Módulos cargados (primeros 15) ==="
ls /sys/module/ | head -15
echo "..."
echo "Total: $(ls /sys/module/ | wc -l) módulos"

echo ""
echo "=== Parámetros del módulo loop ==="
ls /sys/module/loop/parameters/ 2>/dev/null || echo "módulo loop no encontrado"
for p in /sys/module/loop/parameters/*; do
    [ -f "$p" ] && echo "  $(basename "$p") = $(cat "$p")"
done 2>/dev/null

echo ""
echo "=== Comparar con lsmod ==="
lsmod 2>/dev/null | head -5 || echo "lsmod no disponible en este contenedor"
'
```

<details>
<summary>Predicción</summary>

`/sys/module/` lista todos los módulos del kernel. En un contenedor, esto
muestra los módulos del **host** (los contenedores comparten el kernel).
Puede haber cientos.

No todos los directorios en `/sys/module/` son módulos cargables — algunos
son componentes compilados directamente en el kernel (built-in). Los módulos
cargables tienen un subdirectorio `parameters/`.

Los parámetros del módulo `loop`:
- `max_loop` = 0 (sin límite en kernels modernos)
- `max_part` = 0 (número máximo de particiones por dispositivo loop)

`lsmod` puede no estar disponible en el contenedor (depende del paquete
`kmod`). Si está, muestra nombre, tamaño, y qué módulos dependen de él.
`/sys/module/` y `lsmod` obtienen la misma información del kernel.

</details>

---

### Ejercicio 7 — Comparar /proc y /sys para información de red

Demuestra que ambos filesystems exponen los mismos datos de red.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== /proc/net/dev (formato legacy, tabla compacta) ==="
cat /proc/net/dev

echo ""
echo "=== /sys/class/net/ (formato moderno, un archivo por valor) ==="
for iface in /sys/class/net/*; do
    name=$(basename "$iface")
    rx=$(cat "$iface/statistics/rx_bytes")
    tx=$(cat "$iface/statistics/tx_bytes")
    echo "$name: rx_bytes=$rx tx_bytes=$tx"
done

echo ""
echo "=== ip -s link (herramienta de alto nivel, usa netlink) ==="
ip -s link show eth0 2>/dev/null | head -6
'
```

<details>
<summary>Predicción</summary>

Los tres métodos muestran los mismos datos de red pero con interfaces
diferentes:

**`/proc/net/dev`** — formato tabular compacto con todas las interfaces y
estadísticas en una tabla. Requiere parseo (columnas separadas por espacios,
header de 2 líneas). Es la interfaz legacy.

**`/sys/class/net/*/statistics/`** — un archivo por valor (`rx_bytes`,
`tx_bytes`, etc.). Fácil de leer desde scripts (`cat` un archivo = un valor).
Es la interfaz moderna de sysfs.

**`ip -s link`** — herramienta de alto nivel que usa **netlink sockets**
(no lee de `/proc` ni de `/sys/`). Netlink es una interfaz de socket del kernel
para comunicación con subsistemas de red. Es la interfaz más robusta y la
recomendada para herramientas de red.

Las tres fuentes consultan al kernel los mismos contadores internos, pero a
través de interfaces diferentes:
- `/proc/net/dev` → procfs
- `/sys/class/net/` → sysfs
- `ip` → netlink

</details>

---

### Ejercicio 8 — Explorar /sys/fs/cgroup

Examina los cgroups que Docker usa para limitar recursos.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== /sys/fs/cgroup ==="
ls /sys/fs/cgroup/ 2>/dev/null | head -15

echo ""
echo "=== Tipo de cgroup ==="
mount | grep cgroup

echo ""
echo "=== Límites de memoria (si existen) ==="
cat /sys/fs/cgroup/memory.max 2>/dev/null || \
cat /sys/fs/cgroup/memory/memory.limit_in_bytes 2>/dev/null || \
echo "No se encontraron límites de memoria en cgroup"

echo ""
echo "=== Uso actual de memoria ==="
cat /sys/fs/cgroup/memory.current 2>/dev/null || \
cat /sys/fs/cgroup/memory/memory.usage_in_bytes 2>/dev/null || \
echo "No se encontró uso de memoria en cgroup"
'
```

<details>
<summary>Predicción</summary>

`/sys/fs/cgroup/` expone los controladores de cgroups que Docker usa para
limitar recursos del contenedor.

En **cgroups v2** (sistemas modernos): los archivos están directamente en
`/sys/fs/cgroup/`:
- `memory.max` — límite de memoria (`max` si no hay límite)
- `memory.current` — uso actual
- `cpu.max` — límite de CPU

En **cgroups v1** (sistemas más antiguos): hay subdirectorios por controlador:
- `/sys/fs/cgroup/memory/memory.limit_in_bytes`
- `/sys/fs/cgroup/cpu/cpu.cfs_quota_us`

`mount | grep cgroup` muestra si se usa v1 (múltiples montajes de tipo
`cgroup`) o v2 (un solo montaje de tipo `cgroup2`).

Si el compose.yml no define `mem_limit`, `memory.max` muestra `max` (sin
límite). El uso actual (`memory.current`) muestra cuánta memoria usa
realmente el contenedor (independientemente de `/proc/meminfo` que muestra
la memoria del host).

</details>

---

### Ejercicio 9 — /sys en contenedores vs host

Observa qué información de `/sys` es visible desde un contenedor.

```bash
echo "=== Visibilidad desde contenedor ==="
docker compose exec -T debian-dev bash -c '
echo "Categorías de /sys:"
ls /sys/

echo ""
echo "Dispositivos de bloque:"
ls /sys/block/ 2>/dev/null || echo "  no accesible"

echo ""
echo "Clases de dispositivos:"
ls /sys/class/ 2>/dev/null | wc -l
echo "  clases visibles"

echo ""
echo "Firmware:"
ls /sys/firmware/ 2>/dev/null || echo "  no accesible"

echo ""
echo "Thermal zones:"
ls /sys/class/thermal/ 2>/dev/null || echo "  no accesible"
'
```

<details>
<summary>Predicción</summary>

Los contenedores Docker tienen acceso **limitado** a `/sys`. El kernel monta
sysfs dentro del contenedor pero restringe el acceso a muchos dispositivos:

- `/sys/block/` — visible (muestra discos del host pero acceso restringido)
- `/sys/class/net/` — visible (muestra interfaces del contenedor)
- `/sys/class/thermal/` — probablemente vacío o no accesible (el contenedor
  no tiene acceso directo a sensores de hardware)
- `/sys/firmware/` — puede estar vacío o inaccesible (ACPI, DMI son del host)

El número de clases visibles es menor que en el host. La mayoría de las clases
relacionadas con hardware físico (leds, backlight, power_supply, hwmon) no
están disponibles dentro del contenedor.

Para acceso completo a `/sys` del host, se necesitaría ejecutar el contenedor
con `--privileged` o montar `/sys` explícitamente, lo cual es un riesgo de
seguridad.

</details>

---

### Ejercicio 10 — Resumen comparativo: /proc vs /sys

Consulta la misma información desde ambos filesystems.

```bash
docker compose exec -T debian-dev bash -c '
echo "╔════════════════════════════════════════════════════════╗"
echo "║             /proc vs /sys - Comparación               ║"
echo "╚════════════════════════════════════════════════════════╝"

echo ""
echo "=== Versión del kernel ==="
echo "/proc: $(cat /proc/version | cut -d" " -f3)"
echo "/sys:  $(cat /sys/module/kernel/parameters/* 2>/dev/null | head -1 || echo "n/a")"
echo "uname: $(uname -r)"

echo ""
echo "=== Interfaces de red ==="
echo "/proc/net/dev:"
cat /proc/net/dev | tail -n +3 | awk "{print \"  \" \$1}"

echo ""
echo "/sys/class/net/:"
ls /sys/class/net/ | sed "s/^/  /"

echo ""
echo "=== Discos ==="
echo "/proc/partitions:"
cat /proc/partitions 2>/dev/null | tail -n +3 | awk "{print \"  \" \$4}" | head -5

echo ""
echo "/sys/block/:"
ls /sys/block/ 2>/dev/null | sed "s/^/  /" | head -5

echo ""
echo "=== Módulos ==="
echo "/proc/modules (primeros 3):"
cat /proc/modules 2>/dev/null | head -3 | awk "{print \"  \" \$1}"

echo ""
echo "/sys/module/ (primeros 3):"
ls /sys/module/ | head -3 | sed "s/^/  /"
'
```

<details>
<summary>Predicción</summary>

Ambos filesystems reportan la misma información porque consultan al mismo
kernel, pero con formatos diferentes:

**Red**: `/proc/net/dev` muestra una tabla con todas las interfaces y sus
estadísticas en columnas. `/sys/class/net/` muestra un directorio por interfaz
con archivos individuales por métrica. Las interfaces listadas son las mismas.

**Discos**: `/proc/partitions` muestra una tabla con major, minor, bloques y
nombre. `/sys/block/` muestra un directorio por dispositivo con subdirectorios
detallados. `/proc/partitions` incluye particiones, `/sys/block/` solo
dispositivos completos (las particiones están como subdirectorios).

**Módulos**: `/proc/modules` muestra una tabla con nombre, tamaño, uso y
dependencias. `/sys/module/` muestra un directorio por módulo con
subdirectorios de parámetros. `/sys/module/` puede listar más entradas porque
incluye componentes built-in del kernel, no solo módulos cargables.

**Diseño**: `/proc` heredó una mezcla de información de procesos y
dispositivos. `/sys` fue diseñado específicamente para organizar dispositivos
en una jerarquía limpia. La tendencia es que nueva funcionalidad de
dispositivos se expone solo en `/sys`.

</details>

---

## Resumen de conceptos

| Concepto | Detalle clave |
|---|---|
| `/sys` (sysfs) | Filesystem virtual para modelo de dispositivos del kernel |
| `/sys/class/` | Dispositivos agrupados por función (net, block, tty) |
| `/sys/devices/` | Jerarquía física real; destino de todos los symlinks |
| `/sys/block/` | Dispositivos de bloque: tamaño, scheduler, particiones |
| `/sys/module/` | Módulos del kernel: parámetros, estado |
| `/sys/fs/cgroup/` | Límites de recursos (usado por Docker/systemd) |
| Symlinks | `/sys/class/` → `/sys/devices/`; lógico → físico |
| Diseño sysfs | Un valor por archivo; fácil de leer desde scripts |
| `/proc` vs `/sys` | `/proc` = legacy + procesos; `/sys` = moderno + dispositivos |
| `ip` usa netlink | No lee de `/sys/`; interfaz de socket independiente |
