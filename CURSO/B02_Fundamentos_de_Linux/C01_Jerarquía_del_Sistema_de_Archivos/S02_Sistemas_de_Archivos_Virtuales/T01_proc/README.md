# T01 — /proc

## Qué es /proc

`/proc` es un **filesystem virtual** (procfs) que no existe en disco. El kernel
lo genera dinámicamente en memoria y lo monta en `/proc` durante el arranque.
Cada lectura de un archivo en `/proc` es una consulta en tiempo real al kernel.

```bash
mount | grep proc
# proc on /proc type proc (rw,nosuid,nodev,noexec,relatime)

df -h /proc
# Filesystem      Size  Used Avail Use%  Mounted on
# proc               0     0     0    -  /proc
```

El tamaño es 0 porque no ocupa espacio real en disco — los "archivos" se
generan al momento de leerlos.

## Información de procesos: /proc/[pid]

Cada proceso en ejecución tiene un directorio numérico en `/proc` con su PID:

```bash
ls /proc/ | grep '^[0-9]'
# 1
# 2
# 145
# 3021
# ...
```

### Archivos principales de /proc/[pid]

```bash
# Ejemplo con PID 1 (init/systemd)
ls /proc/1/
```

| Archivo | Contenido |
|---|---|
| `cmdline` | Línea de comandos completa (separada por NUL bytes) |
| `status` | Estado legible: nombre, estado, UID, GID, memoria, threads |
| `stat` | Estado en formato compacto (para parseo programático) |
| `environ` | Variables de entorno (separadas por NUL bytes) |
| `exe` | Symlink al binario del ejecutable |
| `cwd` | Symlink al directorio de trabajo actual |
| `root` | Symlink al directorio raíz (/ o chroot) |
| `fd/` | Directorio con symlinks a cada file descriptor abierto |
| `maps` | Mapa de memoria virtual (regiones mapeadas, bibliotecas) |
| `limits` | Límites de recursos del proceso (RLIMIT) |
| `io` | Estadísticas de I/O (bytes leídos/escritos) |
| `ns/` | Namespaces del proceso |
| `mountinfo` | Puntos de montaje visibles por el proceso |

```bash
# Ver la línea de comandos de un proceso
cat /proc/1/cmdline | tr '\0' ' '
# /sbin/init

# Ver estado legible
cat /proc/1/status | head -10
# Name:   systemd
# State:  S (sleeping)
# Tgid:   1
# Pid:    1
# PPid:   0
# Uid:    0   0   0   0
# Gid:    0   0   0   0
# ...

# Ver file descriptors abiertos
ls -la /proc/1/fd/ | head -5
# lr-x------ 1 root root 64 ... 0 -> /dev/null
# lw-x------ 1 root root 64 ... 1 -> /dev/null
# lw-x------ 1 root root 64 ... 2 -> /dev/null
# ...

# Ver el binario que ejecuta
readlink /proc/1/exe
# /usr/lib/systemd/systemd

# Ver el directorio de trabajo
readlink /proc/1/cwd
# /
```

### Variables de entorno de un proceso

```bash
# Ver las variables de entorno de un proceso
cat /proc/<pid>/environ | tr '\0' '\n'
# HOME=/root
# PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin
# LANG=en_US.UTF-8
# ...

# Buscar una variable específica
cat /proc/<pid>/environ | tr '\0' '\n' | grep PATH
```

Esto permite ver las variables de entorno de **cualquier proceso**, no solo la
shell actual. Útil para debugging de servicios.

### Mapa de memoria

```bash
cat /proc/<pid>/maps | head -5
# 5577c8a00000-5577c8a02000 r--p 00000000 08:01 131089  /usr/bin/bash
# 5577c8a02000-5577c8adb000 r-xp 00002000 08:01 131089  /usr/bin/bash
# 5577c8adb000-5577c8b12000 r--p 000db000 08:01 131089  /usr/bin/bash
# ...
```

Las columnas son: rango de direcciones, permisos (r=read, w=write, x=execute,
p=private/s=shared), offset, dispositivo, inodo, pathname.

## /proc/self — Autoreferencia

`/proc/self` es un symlink mágico que siempre apunta al `/proc/[pid]` del
proceso que lo lee:

```bash
readlink /proc/self
# 12345  (el PID del proceso cat/readlink)

# Cada ejecución da un PID diferente
readlink /proc/self
# 12346

# Ver el binario que ejecuta la shell actual
readlink /proc/self/exe
# /usr/bin/bash

# Ver los file descriptors de la shell
ls -la /proc/self/fd/
# lr-x------ ... 0 -> /dev/pts/0
# lw-x------ ... 1 -> /dev/pts/0
# lw-x------ ... 2 -> /dev/pts/0
# lr-x------ ... 255 -> /dev/pts/0
```

`/proc/self` es la forma portátil de que un proceso se inspeccione a sí mismo
sin necesitar conocer su propio PID. Los programas en C usan esto
frecuentemente para leer `/proc/self/maps`, `/proc/self/exe`, etc.

## Información global del sistema

Además de los directorios por proceso, `/proc` contiene archivos globales:

### Hardware y recursos

```bash
# Información de CPU
cat /proc/cpuinfo | grep "model name" | head -1
# model name : Intel(R) Core(TM) i7-10700K CPU @ 3.80GHz

# Número de CPUs
grep -c ^processor /proc/cpuinfo
# 8

# Memoria del sistema
cat /proc/meminfo | head -5
# MemTotal:       16384000 kB
# MemFree:         8234568 kB
# MemAvailable:   12456789 kB
# Buffers:          234567 kB
# Cached:          3456789 kB

# Uptime (en segundos)
cat /proc/uptime
# 345678.90 2765432.10
# primer valor: tiempo encendido, segundo: tiempo idle total (suma de CPUs)

# Load average
cat /proc/loadavg
# 0.15 0.10 0.05 1/234 12345
# 1min 5min 15min ejecutando/total último_pid
```

### Sistema

```bash
# Versión del kernel
cat /proc/version
# Linux version 6.1.0-18-amd64 (debian-kernel@lists.debian.org) (gcc-12 ...)

# Filesystems soportados por el kernel
cat /proc/filesystems
# nodev   sysfs
# nodev   tmpfs
# nodev   proc
#         ext4
#         xfs
# ...
# "nodev" significa que no requiere dispositivo de bloque

# Puntos de montaje actuales
cat /proc/mounts | head -5
# sysfs /sys sysfs rw,nosuid,nodev,noexec,relatime 0 0
# proc /proc proc rw,nosuid,nodev,noexec,relatime 0 0
# ...

# Particiones
cat /proc/partitions
# major minor  #blocks  name
#   8        0  500107608 sda
#   8        1     524288 sda1
#   8        2  499582336 sda2
```

### Red

```bash
# Interfaces de red y estadísticas
cat /proc/net/dev
# Inter-|   Receive                            |  Transmit
#  face |bytes    packets ...                   |bytes    packets ...
#   lo: 12345678  123456 ...                     12345678  123456 ...
#  eth0: 98765432 654321 ...                     45678901  234567 ...

# Conexiones TCP activas
cat /proc/net/tcp | head -3

# Tabla ARP
cat /proc/net/arp
```

## /proc/sys — Parámetros del kernel

`/proc/sys` es especial porque sus archivos son **legibles y escribibles**.
Permite modificar el comportamiento del kernel en tiempo real sin reiniciar.

### Estructura

```
/proc/sys/
├── kernel/   ← Parámetros generales del kernel
├── net/      ← Parámetros de red
├── vm/       ← Parámetros de memoria virtual
├── fs/       ← Parámetros de filesystems
└── dev/      ← Parámetros de dispositivos
```

### Parámetros comunes del kernel

```bash
# Nombre del host (equivale a hostname)
cat /proc/sys/kernel/hostname
# webserver01

# Versión del kernel
cat /proc/sys/kernel/osrelease
# 6.1.0-18-amd64

# Rango de PIDs
cat /proc/sys/kernel/pid_max
# 4194304

# Permitir que usuarios no-root hagan ping (ICMP)
cat /proc/sys/net/ipv4/ping_group_range
# 0   2147483647
```

### Parámetros de red

```bash
# IP forwarding (router)
cat /proc/sys/net/ipv4/ip_forward
# 0 (deshabilitado)

# Para habilitarlo temporalmente:
echo 1 > /proc/sys/net/ipv4/ip_forward

# Rango de puertos efímeros
cat /proc/sys/net/ipv4/ip_local_port_range
# 32768   60999

# SYN cookies (protección contra SYN flood)
cat /proc/sys/net/ipv4/tcp_syncookies
# 1
```

### Parámetros de memoria

```bash
# Swappiness (0-100, cuánto prefiere swap vs liberar cache)
cat /proc/sys/vm/swappiness
# 60

# Overcommit de memoria
cat /proc/sys/vm/overcommit_memory
# 0 (heurístico — default)

# Máximo de archivos mapeados en memoria
cat /proc/sys/vm/max_map_count
# 65530
```

### Parámetros de filesystem

```bash
# Máximo de file descriptors del sistema
cat /proc/sys/fs/file-max
# 9223372036854775807

# File descriptors en uso
cat /proc/sys/fs/file-nr
# 3456  0  9223372036854775807
# en_uso  asignados_libres  máximo
```

### Modificar parámetros

Hay dos formas de modificar parámetros de `/proc/sys`:

```bash
# Método 1: escribir directamente (temporal — se pierde al reiniciar)
echo 1 > /proc/sys/net/ipv4/ip_forward

# Método 2: sysctl (equivalente, más legible)
sysctl net.ipv4.ip_forward=1
# net.ipv4.ip_forward = 1

# Leer con sysctl
sysctl net.ipv4.ip_forward
# net.ipv4.ip_forward = 0

# Ver todos los parámetros
sysctl -a | wc -l
# ~1500 parámetros
```

La notación de sysctl usa puntos en vez de barras:
`/proc/sys/net/ipv4/ip_forward` → `net.ipv4.ip_forward`

### Persistir cambios

Los cambios con `echo` o `sysctl` son **temporales**. Para que sobrevivan un
reinicio, se configuran en archivos de sysctl:

```bash
# Archivo principal
cat /etc/sysctl.conf

# Fragmentos modulares (patrón .d/)
ls /etc/sysctl.d/
# 99-sysctl.conf
# 10-custom.conf

# Ejemplo: hacer ip_forward permanente
echo "net.ipv4.ip_forward = 1" | sudo tee /etc/sysctl.d/99-ip-forward.conf

# Aplicar sin reiniciar
sudo sysctl --system
# o solo un archivo:
sudo sysctl -p /etc/sysctl.d/99-ip-forward.conf
```

## /proc en contenedores

Dentro de un contenedor Docker, `/proc` muestra información **parcialmente
aislada** por namespaces:

```bash
# Dentro de un contenedor, /proc/1 es el proceso principal del contenedor
docker run --rm debian cat /proc/1/cmdline | tr '\0' ' '
# cat /proc/1/cmdline

# Pero /proc/cpuinfo y /proc/meminfo muestran los recursos del HOST
docker run --rm debian cat /proc/cpuinfo | grep -c processor
# 8  (los CPUs del host, no del contenedor)
```

Esto tiene implicaciones importantes: las aplicaciones en contenedores que leen
`/proc/meminfo` para dimensionar su heap (como la JVM) pueden sobredimensionarse
al ver la memoria total del host. Herramientas como LXCFS resuelven esto
virtualizando `/proc` dentro del contenedor.

---

## Ejercicios

### Ejercicio 1 — Inspeccionar un proceso

```bash
# Encontrar el PID de bash
echo $$

# Inspeccionar tu propia shell
cat /proc/$$/status | head -10
cat /proc/$$/cmdline | tr '\0' ' '
readlink /proc/$$/exe
ls -la /proc/$$/fd/
```

### Ejercicio 2 — Información del sistema

```bash
# Cuántas CPUs tiene el sistema
grep -c ^processor /proc/cpuinfo

# Cuánta memoria total y disponible
grep -E "MemTotal|MemAvailable" /proc/meminfo

# Cuánto tiempo lleva encendido
cat /proc/uptime | awk '{printf "%.0f horas\n", $1/3600}'
```

### Ejercicio 3 — Parámetros del kernel

```bash
# Leer swappiness actual
sysctl vm.swappiness

# Leer el máximo de file descriptors
sysctl fs.file-max

# Ver cuántos parámetros existen en /proc/sys
sysctl -a 2>/dev/null | wc -l
```
