# T01 — /proc

## Objetivo

Explorar el filesystem virtual `/proc`: inspeccionar procesos por PID, usar
`/proc/self` para autoreferencia, leer información de hardware y sistema, y
consultar/modificar parámetros del kernel con `sysctl`.

---

## Errores corregidos respecto al material original

1. **Permisos de file descriptors en `/proc/[pid]/fd/`** — El original muestra
   `lw-x------` para stdout y stderr. Esto es imposible: la posición 1 en
   permisos Unix es lectura (`r` o `-`), no escritura. El formato correcto es:
   - Lectura: `lr-x------` (stdin de `/dev/null`)
   - Escritura: `l-wx------` (stdout/stderr a `/dev/null`)
   - Lectura+escritura: `lrwx------` (terminal `/dev/pts/N`)

2. **Campo medio de `/proc/sys/fs/file-nr`** — El original describe los tres
   campos como "en_uso  asignados_libres  máximo". El campo medio es siempre
   0 en kernels modernos (desde Linux 2.6) porque el kernel recicla file
   descriptors inmediatamente. El campo existe por compatibilidad histórica.

---

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

El tamaño es 0 porque no ocupa espacio real — los "archivos" se generan al
momento de leerlos.

## Información de procesos: /proc/[pid]

Cada proceso en ejecución tiene un directorio numérico con su PID:

```bash
ls /proc/ | grep '^[0-9]'
# 1
# 2
# 145
# ...
```

### Archivos principales de /proc/[pid]

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
# Name:   bash
# State:  S (sleeping)
# Pid:    1
# PPid:   0
# Uid:    0   0   0   0
# Gid:    0   0   0   0

# Ver el binario que ejecuta
readlink /proc/1/exe
# /usr/bin/bash

# Ver el directorio de trabajo
readlink /proc/1/cwd
# /
```

### File descriptors: /proc/[pid]/fd/

```bash
ls -la /proc/1/fd/ | head -5
```

Los permisos del symlink reflejan el modo de apertura del fd:

| Permisos | Significado | Ejemplo |
|---|---|---|
| `lr-x------` | Solo lectura (O_RDONLY) | stdin desde archivo |
| `l-wx------` | Solo escritura (O_WRONLY) | stdout a archivo |
| `lrwx------` | Lectura+escritura (O_RDWR) | terminal `/dev/pts/N` |

Los fd estándar: 0 = stdin, 1 = stdout, 2 = stderr.

### Variables de entorno de un proceso

```bash
# Ver las variables de entorno de cualquier proceso
cat /proc/<pid>/environ | tr '\0' '\n'
# HOME=/root
# PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin
# LANG=en_US.UTF-8

# Buscar una variable específica
cat /proc/<pid>/environ | tr '\0' '\n' | grep PATH
```

Útil para debugging de servicios: ver qué variables de entorno tiene un
proceso en ejecución.

### Mapa de memoria

```bash
cat /proc/<pid>/maps | head -3
# 5577c8a00000-5577c8a02000 r--p 00000000 08:01 131089  /usr/bin/bash
# 5577c8a02000-5577c8adb000 r-xp 00002000 08:01 131089  /usr/bin/bash
# 5577c8adb000-5577c8b12000 r--p 000db000 08:01 131089  /usr/bin/bash
```

Columnas: rango de direcciones, permisos (r=read, w=write, x=execute,
p=private/s=shared), offset, dispositivo, inodo, pathname.

## /proc/self — Autoreferencia

`/proc/self` es un symlink mágico que siempre apunta al `/proc/[pid]` del
proceso que lo lee:

```bash
readlink /proc/self
# 12345  (el PID del proceso readlink)

# Ver el binario de la shell actual
readlink /proc/self/exe
# /usr/bin/bash

# Ver los file descriptors de la shell
ls -la /proc/self/fd/
# lrwx------ 1 root root 64 ... 0 -> /dev/pts/0
# lrwx------ 1 root root 64 ... 1 -> /dev/pts/0
# lrwx------ 1 root root 64 ... 2 -> /dev/pts/0
```

Los fd de una shell interactiva apuntan a la pseudo-terminal (`/dev/pts/N`)
y son todos `lrwx------` (lectura+escritura) porque las terminales se abren
en modo read-write.

`/proc/self` es la forma portátil de que un proceso se inspeccione a sí mismo
sin necesitar conocer su propio PID. Los programas en C lo usan para leer
`/proc/self/maps`, `/proc/self/exe`, etc.

## Información global del sistema

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
# primer valor: tiempo encendido
# segundo: tiempo idle total (suma de todos los CPUs)

# Load average
cat /proc/loadavg
# 0.15 0.10 0.05 1/234 12345
# 1min 5min 15min ejecutando/total último_pid
```

### Sistema

```bash
# Versión del kernel
cat /proc/version

# Filesystems soportados
cat /proc/filesystems
# "nodev" = no requiere dispositivo de bloque (filesystem virtual)

# Puntos de montaje actuales
cat /proc/mounts | head -5

# Particiones
cat /proc/partitions
```

### Red

```bash
# Interfaces de red y estadísticas
cat /proc/net/dev

# Conexiones TCP activas
cat /proc/net/tcp | head -3

# Tabla ARP
cat /proc/net/arp
```

## /proc/sys — Parámetros del kernel

`/proc/sys` es especial: sus archivos son **legibles y escribibles**. Permite
modificar el comportamiento del kernel en tiempo real sin reiniciar.

### Estructura

```
/proc/sys/
├── kernel/   ← Parámetros generales del kernel
├── net/      ← Parámetros de red
├── vm/       ← Parámetros de memoria virtual
├── fs/       ← Parámetros de filesystems
└── dev/      ← Parámetros de dispositivos
```

### Parámetros comunes

```bash
# Nombre del host
cat /proc/sys/kernel/hostname

# Versión del kernel
cat /proc/sys/kernel/osrelease

# Rango de PIDs
cat /proc/sys/kernel/pid_max
# 4194304

# IP forwarding (0=deshabilitado, 1=habilitado)
cat /proc/sys/net/ipv4/ip_forward

# Rango de puertos efímeros
cat /proc/sys/net/ipv4/ip_local_port_range
# 32768   60999

# Swappiness (0-100)
cat /proc/sys/vm/swappiness
# 60

# Máximo de file descriptors del sistema
cat /proc/sys/fs/file-max

# File descriptors en uso
cat /proc/sys/fs/file-nr
# 3456  0  9223372036854775807
# en_uso  (siempre 0)  máximo
```

### Modificar parámetros

```bash
# Temporal: escribir directamente (se pierde al reiniciar)
echo 1 > /proc/sys/net/ipv4/ip_forward

# Equivalente con sysctl (más legible)
sysctl net.ipv4.ip_forward=1

# Leer con sysctl
sysctl net.ipv4.ip_forward

# Ver todos los parámetros
sysctl -a | wc -l
# ~1500 parámetros
```

La notación sysctl usa puntos en vez de barras:
`/proc/sys/net/ipv4/ip_forward` → `net.ipv4.ip_forward`

### Persistir cambios

Los cambios son **temporales** por defecto. Para que sobrevivan un reinicio:

```bash
# Crear fragmento en /etc/sysctl.d/ (patrón .d/)
echo "net.ipv4.ip_forward = 1" | sudo tee /etc/sysctl.d/99-ip-forward.conf

# Aplicar sin reiniciar
sudo sysctl --system
```

## /proc en contenedores

Dentro de un contenedor Docker, `/proc` muestra información **parcialmente
aislada** por namespaces:

- `/proc/1` es el proceso principal del contenedor (no systemd del host)
- Los directorios de PIDs solo muestran procesos dentro del contenedor
  (PID namespace)
- **Pero** `/proc/cpuinfo` y `/proc/meminfo` muestran los recursos del
  **host**, no del contenedor

```bash
docker run --rm debian cat /proc/cpuinfo | grep -c processor
# 8  (los CPUs del host, no del contenedor)
```

Esto puede causar que aplicaciones sobredimensionen sus recursos (la JVM, por
ejemplo, lee `/proc/meminfo` para dimensionar su heap). Herramientas como
LXCFS virtualizan `/proc` dentro del contenedor para resolver esto.

---

## Ejercicios

Todos los comandos se ejecutan dentro de los contenedores del curso.

### Ejercicio 1 — Inspeccionar PID 1 del contenedor

Examina el proceso principal del contenedor usando `/proc/1/`.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Línea de comandos ==="
cat /proc/1/cmdline | tr "\0" " "
echo ""

echo ""
echo "=== Status ==="
cat /proc/1/status | head -10

echo ""
echo "=== Ejecutable ==="
readlink /proc/1/exe

echo ""
echo "=== Directorio de trabajo ==="
readlink /proc/1/cwd

echo ""
echo "=== File descriptors ==="
ls -la /proc/1/fd/ 2>/dev/null | head -5
'
```

<details>
<summary>Predicción</summary>

PID 1 en un contenedor Docker es el proceso definido en el `CMD` o `ENTRYPOINT`
del Dockerfile (probablemente `bash` o `sleep infinity` en este lab).

`status` muestra:
- `Name`: nombre corto del proceso
- `State: S (sleeping)` — duerme esperando input
- `Pid: 1`, `PPid: 0` — es el proceso raíz del contenedor

`exe` apunta al binario (`/usr/bin/bash` o similar). `cwd` es `/` (o el
`WORKDIR` del Dockerfile).

Los file descriptors de PID 1 pueden apuntar a `/dev/null` o a pipes de
Docker, dependiendo de cómo se configuró el contenedor. Si apuntan a
`/dev/null`, los permisos serán `lr-x------` (stdin, read-only) y
`l-wx------` (stdout/stderr, write-only).

</details>

---

### Ejercicio 2 — Usar /proc/self para autoinspección

Observa cómo `/proc/self` cambia dependiendo del proceso que lo lee.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== PID de esta shell ==="
echo "PID: $$"
echo "/proc/self apunta a: $(readlink /proc/self)"

echo ""
echo "=== Ejecutable ==="
readlink /proc/self/exe

echo ""
echo "=== File descriptors ==="
ls -la /proc/self/fd/

echo ""
echo "=== Mapa de memoria (primeras 5 regiones) ==="
head -5 /proc/self/maps
'
```

<details>
<summary>Predicción</summary>

`$$` muestra el PID de la shell bash. `/proc/self` apunta a un PID
**diferente** — el PID del proceso `readlink` que ejecuta la lectura, no el
de bash. Cada comando que lee `/proc/self` obtiene su propio PID.

`/proc/self/exe` apunta a `/usr/bin/bash` (la shell que ejecuta el script).

Los file descriptors muestran `lrwx------` si la shell está conectada a una
terminal (`/dev/pts/N`, modo read-write). Sin `-T` en `docker compose exec`,
los fds apuntarían a pipes.

El mapa de memoria muestra las regiones de bash: el binario cargado, sus
bibliotecas (`libc.so`, `libreadline.so`), el heap, el stack, y regiones
anónimas (`[heap]`, `[stack]`, `[vdso]`). Las columnas son: rango de
direcciones, permisos (`r-xp` = código ejecutable, `rw-p` = datos
modificables), y el archivo mapeado.

</details>

---

### Ejercicio 3 — Leer información de hardware del host

Consulta CPU, memoria y uptime a través de `/proc`.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== CPU ==="
grep "model name" /proc/cpuinfo | head -1
echo "Procesadores: $(grep -c "^processor" /proc/cpuinfo)"

echo ""
echo "=== Memoria ==="
grep -E "MemTotal|MemFree|MemAvailable" /proc/meminfo

echo ""
echo "=== Uptime ==="
cat /proc/uptime
echo "Horas encendido: $(awk "{printf \"%.1f\", \$1/3600}" /proc/uptime)"

echo ""
echo "=== Load average ==="
cat /proc/loadavg
'
```

<details>
<summary>Predicción</summary>

Toda esta información es del **host**, no del contenedor. Los contenedores
comparten el kernel del host y `/proc/cpuinfo`, `/proc/meminfo`, `/proc/uptime`
no están virtualizados por namespaces.

- **CPU**: muestra el modelo y cantidad de procesadores del host
- **Memoria**: `MemTotal` es la RAM total del host (no el limit del contenedor
  si se configuró uno con `mem_limit` en compose.yml)
- **Uptime**: tiempo desde el último arranque del host (no desde que se creó
  el contenedor)
- **Load average**: tres valores (1, 5, 15 minutos). Valores menores al número
  de CPUs indican que el sistema no está saturado

Esta es una limitación conocida de `/proc` en contenedores. Herramientas
conscientes de cgroups (como `free` en kernels recientes) pueden reportar
valores correctos usando `/sys/fs/cgroup/`.

</details>

---

### Ejercicio 4 — Explorar variables de entorno de procesos

Inspecciona las variables de entorno de procesos en ejecución.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Variables de entorno de PID 1 ==="
cat /proc/1/environ 2>/dev/null | tr "\0" "\n" | head -10

echo ""
echo "=== Variables de la shell actual (via /proc/self) ==="
cat /proc/self/environ | tr "\0" "\n" | head -10

echo ""
echo "=== Buscar PATH de PID 1 ==="
cat /proc/1/environ 2>/dev/null | tr "\0" "\n" | grep "^PATH="
'
```

<details>
<summary>Predicción</summary>

Las variables de PID 1 incluyen las definidas en el Dockerfile (`ENV`), las
inyectadas por Docker (como `HOSTNAME`, `HOME`), y las del compose.yml
(`environment:`).

Las variables de `/proc/self` son las de la shell actual, que hereda las del
contenedor más las del entorno de `docker compose exec`.

Los archivos `environ` usan NUL (`\0`) como separador entre variables (no
newline). `tr "\0" "\n"` convierte a formato legible.

`/proc/[pid]/environ` es una herramienta de debugging poderosa: permite ver
las variables de entorno de **cualquier proceso** sin interrumpirlo. Útil
para diagnosticar problemas de configuración en servicios (¿el servicio ve el
`DATABASE_URL` correcto?).

> Nota: `environ` muestra las variables al momento de la creación del proceso.
> Si el proceso modifica sus variables internamente (con `setenv()`), los
> cambios pueden no reflejarse aquí en todos los kernels.

</details>

---

### Ejercicio 5 — Comparar /proc/version entre contenedores

Demuestra que los contenedores comparten el kernel del host.

```bash
echo "=== Debian ==="
docker compose exec -T debian-dev bash -c 'cat /proc/version'

echo ""
echo "=== AlmaLinux ==="
docker compose exec -T alma-dev bash -c 'cat /proc/version'

echo ""
echo "=== Host ==="
cat /proc/version 2>/dev/null || echo "(ejecutar en el host para comparar)"
```

<details>
<summary>Predicción</summary>

Ambos contenedores muestran **exactamente la misma versión del kernel**. Esto
demuestra que los contenedores no tienen su propio kernel — comparten el del
host.

`/proc/version` muestra algo como:
`Linux version 6.x.y-... (gcc version X.Y.Z ...)`

La imagen Debian y la imagen AlmaLinux tienen diferentes userlands (diferentes
versiones de glibc, gcc, utilidades), pero ejecutan sobre el mismo kernel. Por
eso un contenedor Debian puede correr en un host con kernel de Fedora, y
viceversa.

Si ejecutas `cat /proc/version` en el host, obtienes el mismo resultado.
Los tres comandos son consultas al mismo kernel.

</details>

---

### Ejercicio 6 — Explorar /proc/sys con sysctl

Consulta parámetros del kernel usando ambos métodos: lectura directa y sysctl.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Método 1: lectura directa de /proc/sys ==="
echo "Hostname:    $(cat /proc/sys/kernel/hostname)"
echo "Kernel:      $(cat /proc/sys/kernel/osrelease)"
echo "PID max:     $(cat /proc/sys/kernel/pid_max)"
echo "IP forward:  $(cat /proc/sys/net/ipv4/ip_forward)"

echo ""
echo "=== Método 2: sysctl (puntos en vez de barras) ==="
sysctl kernel.hostname
sysctl kernel.osrelease
sysctl kernel.pid_max
sysctl net.ipv4.ip_forward

echo ""
echo "=== Categorías de /proc/sys ==="
ls /proc/sys/

echo ""
echo "=== Total de parámetros ==="
echo "$(sysctl -a 2>/dev/null | wc -l) parámetros ajustables"
'
```

<details>
<summary>Predicción</summary>

Ambos métodos devuelven los mismos valores. La diferencia es solo de notación:
- Lectura directa: `/proc/sys/kernel/hostname` (barras)
- sysctl: `kernel.hostname` (puntos)

`sysctl` es la herramienta recomendada porque:
- Salida formateada (`kernel.hostname = valor`)
- Puede listar todos los parámetros (`sysctl -a`)
- Puede aplicar configuraciones desde archivos (`sysctl -p`)

Las categorías de `/proc/sys/`: `kernel/`, `net/`, `vm/`, `fs/`, `dev/`.
Hay más de 1000 parámetros ajustables. La mayoría tiene valores por defecto
razonables.

`ip_forward` es probablemente `1` dentro del contenedor (Docker habilita
forwarding para el networking de contenedores) y `1` en el host por la misma
razón.

</details>

---

### Ejercicio 7 — Parámetros de red y memoria

Consulta parámetros específicos de red y memoria virtual.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Parámetros de red ==="
echo "IP forward:        $(sysctl -n net.ipv4.ip_forward)"
echo "Puertos efímeros:  $(sysctl -n net.ipv4.ip_local_port_range)"
echo "SYN cookies:       $(sysctl -n net.ipv4.tcp_syncookies)"
echo "Keepalive time:    $(sysctl -n net.ipv4.tcp_keepalive_time) segundos"

echo ""
echo "=== Parámetros de memoria ==="
echo "Swappiness:        $(sysctl -n vm.swappiness)"
echo "Overcommit:        $(sysctl -n vm.overcommit_memory)"
echo "Max map count:     $(sysctl -n vm.max_map_count)"

echo ""
echo "=== Parámetros de filesystem ==="
echo "File-max:          $(sysctl -n fs.file-max)"
echo "File-nr:           $(cat /proc/sys/fs/file-nr)"
echo "  (formato: en_uso  siempre_0  máximo)"
'
```

<details>
<summary>Predicción</summary>

`sysctl -n` muestra solo el valor (sin el nombre del parámetro). Útil para
scripts.

**Red**:
- `ip_forward = 1` — Docker lo habilita para routing entre contenedores
- `ip_local_port_range = 32768 60999` — rango de puertos efímeros para
  conexiones salientes
- `tcp_syncookies = 1` — protección contra ataques SYN flood habilitada
- `tcp_keepalive_time = 7200` — TCP keepalive cada 2 horas (default)

**Memoria**:
- `swappiness = 60` — balance entre swap y liberación de cache (0=evitar swap,
  100=preferir swap)
- `overcommit_memory = 0` — el kernel usa heurísticas para decidir si permite
  overcommit
- `max_map_count = 65530` — máximo de regiones de memoria mapeadas por proceso

**Filesystem**:
- `file-max` — límite global de file descriptors (valor enorme en kernels
  modernos: 2^63-1)
- `file-nr` — fd en uso actualmente / 0 (legacy, siempre 0) / máximo

Estos parámetros son globales del host — los contenedores los comparten.

</details>

---

### Ejercicio 8 — Filesystems soportados y montajes

Consulta qué filesystems conoce el kernel y qué está montado.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Filesystems soportados ==="
cat /proc/filesystems | head -15
echo ""
echo "Total: $(wc -l < /proc/filesystems) filesystems"
echo "  Virtuales (nodev): $(grep -c "nodev" /proc/filesystems)"
echo "  De bloque:         $(grep -cv "nodev" /proc/filesystems)"

echo ""
echo "=== Puntos de montaje del contenedor ==="
cat /proc/mounts | head -10
'
```

<details>
<summary>Predicción</summary>

`/proc/filesystems` lista todos los tipos de filesystem que el kernel soporta.
Los marcados con `nodev` son virtuales (no necesitan un dispositivo de bloque):
`proc`, `sysfs`, `tmpfs`, `cgroup2`, `overlay`, etc.

Los que no tienen `nodev` son filesystems de bloque: `ext4`, `xfs`, `btrfs`,
`vfat`, etc. Estos necesitan un dispositivo (`/dev/sda1`, etc.).

`/proc/mounts` muestra los montajes actuales del contenedor. El filesystem
principal es `overlay` (Docker usa overlay2 como storage driver). También se
ven:
- `proc on /proc type proc`
- `tmpfs on /dev type tmpfs`
- `sysfs on /sys type sysfs`
- Bind mounts del `compose.yml` (volúmenes del lab)

`/proc/mounts` es equivalente a `mount` sin argumentos, pero lee directamente
del kernel sin depender de `/etc/mtab`.

</details>

---

### Ejercicio 9 — Comparar /proc entre contenedores y host

Observa qué información es aislada por namespaces y cuál es compartida.

```bash
echo "=== PIDs visibles ==="
echo "Debian (contenedor):"
docker compose exec -T debian-dev bash -c 'ls /proc/ | grep "^[0-9]" | wc -l; echo "  procesos"'
echo "AlmaLinux (contenedor):"
docker compose exec -T alma-dev bash -c 'ls /proc/ | grep "^[0-9]" | wc -l; echo "  procesos"'

echo ""
echo "=== Hostname (aislado por UTS namespace) ==="
echo "Debian:    $(docker compose exec -T debian-dev cat /proc/sys/kernel/hostname)"
echo "AlmaLinux: $(docker compose exec -T alma-dev cat /proc/sys/kernel/hostname)"

echo ""
echo "=== Kernel (compartido) ==="
echo "Debian:    $(docker compose exec -T debian-dev cat /proc/sys/kernel/osrelease)"
echo "AlmaLinux: $(docker compose exec -T alma-dev cat /proc/sys/kernel/osrelease)"

echo ""
echo "=== CPUs (compartido, no aislado) ==="
echo "Debian:    $(docker compose exec -T debian-dev grep -c ^processor /proc/cpuinfo) CPUs"
echo "AlmaLinux: $(docker compose exec -T alma-dev grep -c ^processor /proc/cpuinfo) CPUs"
```

<details>
<summary>Predicción</summary>

**Aislado por namespaces**:
- **PIDs**: cada contenedor ve solo sus propios procesos (PID namespace).
  Probablemente 2-5 procesos en cada uno. El host vería cientos.
- **Hostname**: cada contenedor tiene su propio hostname (UTS namespace),
  definido en el `compose.yml`.

**Compartido (no aislado)**:
- **Kernel**: misma versión en ambos — es el kernel del host
- **CPUs**: mismo número en ambos — ven todos los CPUs del host

Esta asimetría es fundamental para entender contenedores:
- Los namespaces aíslan: PIDs, red, hostname, usuarios, montajes
- Pero `/proc/cpuinfo` y `/proc/meminfo` NO están aislados por defecto
- Los cgroups limitan el **uso** de recursos, pero `/proc` sigue reportando
  los recursos **totales** del host

</details>

---

### Ejercicio 10 — Inspeccionar un proceso completo

Crea un proceso de larga duración y examínalo completamente a través de
`/proc`.

```bash
docker compose exec -T debian-dev bash -c '
# Crear un proceso de fondo
sleep 300 &
PID=$!
echo "Proceso sleep creado con PID: $PID"

echo ""
echo "=== cmdline ==="
cat /proc/$PID/cmdline | tr "\0" " "
echo ""

echo ""
echo "=== status (extracto) ==="
grep -E "^(Name|State|Pid|PPid|Uid|VmSize|VmRSS|Threads):" /proc/$PID/status

echo ""
echo "=== exe ==="
readlink /proc/$PID/exe

echo ""
echo "=== cwd ==="
readlink /proc/$PID/cwd

echo ""
echo "=== environ (primeras 5 variables) ==="
cat /proc/$PID/environ | tr "\0" "\n" | head -5

echo ""
echo "=== fd ==="
ls -la /proc/$PID/fd/

echo ""
echo "=== limits (extracto) ==="
grep -E "open files|processes|file size" /proc/$PID/limits

echo ""
echo "=== Limpiar ==="
kill $PID
echo "Proceso $PID terminado"
'
```

<details>
<summary>Predicción</summary>

`sleep 300 &` crea un proceso en background. `$!` captura su PID.

- `cmdline`: `sleep 300`
- `status`:
  - `Name: sleep`
  - `State: S (sleeping)` — `sleep` pasa todo su tiempo dormido
  - `PPid`: el PID de la shell bash (su proceso padre)
  - `VmSize`: memoria virtual total (pequeña, ~pocos MB)
  - `VmRSS`: memoria residente (aún más pequeña, ~cientos de KB)
  - `Threads: 1` — sleep es single-threaded
- `exe`: `/usr/bin/sleep`
- `cwd`: el directorio de trabajo heredado de la shell
- `environ`: las mismas variables que la shell (heredadas con `fork`)
- `fd`: solo 0, 1, 2 (stdin, stdout, stderr) — sleep no abre archivos
- `limits`:
  - `Max open files`: 1048576 (típico en contenedores Docker)
  - `Max processes`: per-user limit
  - `Max file size`: unlimited

`kill $PID` envía SIGTERM al proceso, terminándolo limpiamente. Después de
esto, `/proc/$PID/` desaparece inmediatamente porque el proceso ya no existe.

</details>

---

## Resumen de conceptos

| Concepto | Detalle clave |
|---|---|
| `/proc` | Filesystem virtual; no ocupa disco; consulta al kernel en tiempo real |
| `/proc/[pid]/` | Info de cada proceso: `status`, `cmdline`, `exe`, `fd/`, `environ` |
| `/proc/self` | Symlink al `/proc/[pid]` del proceso que lo lee |
| `/proc/cpuinfo` | CPUs del host (no aislado en contenedores) |
| `/proc/meminfo` | Memoria del host (no aislado en contenedores) |
| `/proc/sys/` | Parámetros del kernel legibles y escribibles |
| `sysctl` | Herramienta para leer/escribir `/proc/sys/` con notación de puntos |
| Persistir sysctl | Archivos en `/etc/sysctl.d/` + `sysctl --system` |
| fd permissions | `lr-x------`=lectura, `l-wx------`=escritura, `lrwx------`=ambos |
| Contenedores | PIDs aislados, pero cpuinfo/meminfo muestran recursos del host |
