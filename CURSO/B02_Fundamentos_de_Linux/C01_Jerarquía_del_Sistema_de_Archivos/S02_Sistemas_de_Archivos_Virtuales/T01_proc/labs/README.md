# Lab — /proc

## Objetivo

Explorar el filesystem virtual /proc: inspeccionar procesos por PID,
usar /proc/self para autoreferencia, leer informacion de hardware y
sistema, y consultar parametros del kernel con sysctl.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Procesos en /proc

### Objetivo

Inspeccionar la informacion de procesos en ejecucion a traves de
/proc/[pid]/.

### Paso 1.1: Directorios de procesos

```bash
docker compose exec debian-dev bash -c '
echo "=== Directorios numericos (PIDs) ==="
ls /proc/ | grep "^[0-9]" | head -10
echo ""
echo "=== PID 1 (proceso principal del contenedor) ==="
cat /proc/1/cmdline | tr "\0" " "
echo ""
'
```

En un contenedor, PID 1 es el proceso principal (bash en nuestro
caso). En el host, PID 1 es systemd/init.

### Paso 1.2: Status de un proceso

```bash
docker compose exec debian-dev bash -c '
echo "=== Status del PID 1 ==="
cat /proc/1/status | head -15
'
```

Campos importantes: Name (nombre del proceso), State (S=sleeping,
R=running), Pid, PPid (PID del padre), Uid, Gid.

### Paso 1.3: Ejecutable y directorio de trabajo

```bash
docker compose exec debian-dev bash -c '
echo "=== Ejecutable ==="
readlink /proc/1/exe

echo ""
echo "=== Directorio de trabajo ==="
readlink /proc/1/cwd
'
```

`/proc/[pid]/exe` es un symlink al binario. `/proc/[pid]/cwd` apunta
al directorio de trabajo actual del proceso.

### Paso 1.4: File descriptors

```bash
docker compose exec debian-dev bash -c '
echo "=== File descriptors del PID 1 ==="
ls -la /proc/1/fd/ 2>/dev/null | head -10
'
```

Cada entrada en `fd/` es un symlink al archivo o socket que el
proceso tiene abierto. 0=stdin, 1=stdout, 2=stderr.

### Paso 1.5: Variables de entorno

```bash
docker compose exec debian-dev bash -c '
echo "=== Variables de entorno del PID 1 ==="
cat /proc/1/environ 2>/dev/null | tr "\0" "\n" | head -10
'
```

Esto permite ver las variables de entorno de **cualquier proceso**,
no solo la shell actual. Util para debugging de servicios.

---

## Parte 2 — /proc/self

### Objetivo

Usar /proc/self para que un proceso se inspeccione a si mismo.

### Paso 2.1: PID via /proc/self

```bash
docker compose exec debian-dev bash -c '
echo "=== PID de este proceso ==="
echo "PID: $$"
echo "/proc/self apunta a: $(readlink /proc/self)"
'
```

`/proc/self` es un symlink que siempre apunta al `/proc/[pid]` del
proceso que lo lee. Es la forma portable de autoinspeccion.

### Paso 2.2: Ejecutable de la shell

```bash
docker compose exec debian-dev bash -c '
echo "=== Shell actual ==="
readlink /proc/self/exe
echo ""
echo "=== Directorio de trabajo ==="
readlink /proc/self/cwd
'
```

### Paso 2.3: File descriptors de la shell

```bash
docker compose exec debian-dev bash -c '
echo "=== FDs abiertos ==="
ls -la /proc/self/fd/
'
```

La shell tiene al menos stdin (0), stdout (1) y stderr (2) abiertos,
apuntando a la pseudo-terminal (`/dev/pts/N`).

### Paso 2.4: Mapa de memoria

```bash
docker compose exec debian-dev bash -c '
echo "=== Primeras regiones de memoria ==="
head -5 /proc/self/maps
'
```

Muestra las regiones de memoria virtual del proceso: direcciones,
permisos (r/w/x), y las librerias cargadas.

---

## Parte 3 — Informacion del sistema

### Objetivo

Leer informacion de hardware y sistema desde /proc.

### Paso 3.1: CPU

```bash
docker compose exec debian-dev bash -c '
echo "=== CPU ==="
grep "model name" /proc/cpuinfo | head -1
echo ""
echo "Procesadores: $(grep -c "^processor" /proc/cpuinfo)"
'
```

Dentro del contenedor, `/proc/cpuinfo` muestra los CPUs del **host**
(no del contenedor). Esto puede causar que aplicaciones
sobredimensionen su uso de recursos.

### Paso 3.2: Memoria

```bash
docker compose exec debian-dev bash -c '
echo "=== Memoria ==="
grep -E "MemTotal|MemFree|MemAvailable" /proc/meminfo
'
```

Al igual que cpuinfo, `/proc/meminfo` muestra la memoria del host.

### Paso 3.3: Uptime

```bash
docker compose exec debian-dev bash -c '
echo "=== Uptime ==="
cat /proc/uptime
echo ""
echo "Horas encendido: $(awk "{printf \"%.1f\", \$1/3600}" /proc/uptime)"
'
```

El primer valor es el tiempo total encendido (segundos). El segundo
es el tiempo idle acumulado de todos los CPUs.

### Paso 3.4: Version del kernel

```bash
docker compose exec debian-dev bash -c '
echo "=== Kernel ==="
cat /proc/version
'
```

El contenedor comparte el kernel del host. La version mostrada es la
del host, no la de la imagen.

### Paso 3.5: Filesystems soportados

```bash
docker compose exec debian-dev bash -c '
echo "=== Filesystems ==="
cat /proc/filesystems | head -10
echo ""
echo "Total: $(wc -l < /proc/filesystems)"
'
```

Los marcados con `nodev` son filesystems virtuales (no necesitan
dispositivo de bloque): proc, sysfs, tmpfs.

---

## Parte 4 — /proc/sys y sysctl

### Objetivo

Consultar y entender los parametros del kernel modificables en
tiempo real.

### Paso 4.1: Estructura de /proc/sys

```bash
docker compose exec debian-dev bash -c '
echo "=== Categorias de /proc/sys ==="
ls /proc/sys/
'
```

`kernel/` — parametros generales, `net/` — red, `vm/` — memoria
virtual, `fs/` — filesystems.

### Paso 4.2: Leer parametros con cat

```bash
docker compose exec debian-dev bash -c '
echo "=== Hostname ==="
cat /proc/sys/kernel/hostname

echo ""
echo "=== Kernel version ==="
cat /proc/sys/kernel/osrelease

echo ""
echo "=== PID max ==="
cat /proc/sys/kernel/pid_max
'
```

### Paso 4.3: Equivalente con sysctl

```bash
docker compose exec debian-dev bash -c '
echo "=== sysctl usa puntos en vez de barras ==="
sysctl kernel.hostname
sysctl kernel.osrelease
sysctl kernel.pid_max
'
```

La notacion sysctl usa puntos: `/proc/sys/kernel/hostname` se lee
como `kernel.hostname`.

### Paso 4.4: Parametros de red

```bash
docker compose exec debian-dev bash -c '
echo "=== IP forwarding ==="
sysctl net.ipv4.ip_forward

echo ""
echo "=== Puertos efimeros ==="
sysctl net.ipv4.ip_local_port_range

echo ""
echo "=== Swappiness ==="
sysctl vm.swappiness
'
```

### Paso 4.5: Contar parametros

```bash
docker compose exec debian-dev bash -c '
echo "Total de parametros sysctl: $(sysctl -a 2>/dev/null | wc -l)"
'
```

Hay mas de 1000 parametros ajustables. La mayoria tiene valores por
defecto razonables y no necesita modificacion.

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. `/proc` es un filesystem **virtual** — no ocupa espacio en disco.
   Cada lectura es una consulta al kernel en tiempo real.

2. `/proc/[pid]/` contiene informacion de cada proceso: `status`,
   `cmdline`, `exe`, `cwd`, `fd/`, `environ`, `maps`.

3. `/proc/self` es un symlink que siempre apunta al proceso que lo
   lee. Es la forma portable de autoinspeccion sin conocer el PID.

4. Dentro de un contenedor, `/proc/cpuinfo` y `/proc/meminfo`
   muestran los recursos del **host**, no del contenedor.

5. `/proc/sys` contiene parametros del kernel modificables en tiempo
   real. `sysctl` es la herramienta para leerlos y escribirlos,
   usando notacion con puntos (`kernel.hostname`).
