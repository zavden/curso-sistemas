# Lab — Namespaces

## Objetivo

Explorar los namespaces de Linux: inspeccionar los namespaces
del proceso actual via /proc/self/ns/, entender que aisla cada
tipo, y observar el aislamiento que Docker implementa con
namespaces.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

Nota: dentro de un contenedor Docker no se pueden crear
namespaces nuevos (requiere privilegios del host). Este lab se
enfoca en **observar y entender** los namespaces existentes.

---

## Parte 1 — Ver namespaces

### Objetivo

Inspeccionar los namespaces asignados a procesos.

### Paso 1.1: Namespaces del proceso actual

```bash
docker compose exec debian-dev bash -c '
echo "=== /proc/self/ns/ ==="
ls -la /proc/self/ns/
echo ""
echo "Cada enlace apunta a un namespace identificado por inode"
echo "El numero entre corchetes es el inode del namespace"
echo ""
echo "Dos procesos con el MISMO inode estan en el MISMO namespace"
echo "Dos procesos con inodes DIFERENTES estan AISLADOS"
'
```

### Paso 1.2: Comparar namespaces de procesos

```bash
docker compose exec debian-dev bash -c '
echo "=== Namespaces del shell ($$) ==="
ls -la /proc/self/ns/ | awk "{print \$NF}" | sort
echo ""
echo "=== Namespaces de PID 1 ==="
ls -la /proc/1/ns/ 2>/dev/null | awk "{print \$NF}" | sort
echo ""
echo "Si los inodes son iguales → mismos namespaces"
echo "Dentro de un contenedor, todos los procesos"
echo "comparten los mismos namespaces"
'
```

### Paso 1.3: lsns — listar namespaces

```bash
docker compose exec debian-dev bash -c '
echo "=== lsns (listar namespaces) ==="
lsns 2>/dev/null || echo "(lsns no disponible)"
echo ""
echo "Columnas:"
echo "  NS       — inode del namespace"
echo "  TYPE     — tipo (pid, net, mnt, uts, ipc, user, cgroup, time)"
echo "  NPROCS   — procesos en este namespace"
echo "  PID      — PID del proceso lider"
echo "  USER     — usuario del proceso lider"
echo "  COMMAND  — comando del proceso lider"
'
```

### Paso 1.4: lsns por tipo

```bash
docker compose exec debian-dev bash -c '
echo "=== Filtrar por tipo ==="
echo "--- PID namespaces ---"
lsns -t pid 2>/dev/null || echo "(no disponible)"
echo ""
echo "--- Network namespaces ---"
lsns -t net 2>/dev/null || echo "(no disponible)"
echo ""
echo "--- Mount namespaces ---"
lsns -t mnt 2>/dev/null || echo "(no disponible)"
'
```

---

## Parte 2 — Los 8 tipos

### Objetivo

Entender que aisla cada tipo de namespace y como verificarlo.

### Paso 2.1: Los 8 tipos de namespace

```bash
docker compose exec debian-dev bash -c '
echo "=== Los 8 tipos de namespace de Linux ==="
printf "%-10s %-20s %-35s\n" "Tipo" "Flag" "Que aisla"
printf "%-10s %-20s %-35s\n" "--------" "------------------" "---------------------------------"
printf "%-10s %-20s %-35s\n" "Mount"   "CLONE_NEWNS"      "Puntos de montaje"
printf "%-10s %-20s %-35s\n" "UTS"     "CLONE_NEWUTS"     "Hostname y domainname"
printf "%-10s %-20s %-35s\n" "IPC"     "CLONE_NEWIPC"     "Semaforos, colas, shared memory"
printf "%-10s %-20s %-35s\n" "PID"     "CLONE_NEWPID"     "Arbol de PIDs"
printf "%-10s %-20s %-35s\n" "Network" "CLONE_NEWNET"     "Interfaces, rutas, iptables"
printf "%-10s %-20s %-35s\n" "User"    "CLONE_NEWUSER"    "UIDs/GIDs (rootless containers)"
printf "%-10s %-20s %-35s\n" "Cgroup"  "CLONE_NEWCGROUP"  "Vista del arbol de cgroups"
printf "%-10s %-20s %-35s\n" "Time"    "CLONE_NEWTIME"    "Relojes MONOTONIC y BOOTTIME"
'
```

### Paso 2.2: PID namespace — lo que vemos

```bash
docker compose exec debian-dev bash -c '
echo "=== PID namespace: solo vemos nuestros procesos ==="
echo "Total de procesos visibles: $(ps -e --no-headers | wc -l)"
echo ""
ps -eo pid,ppid,cmd | head -10
echo ""
echo "En el host habria cientos de procesos"
echo "Aqui solo vemos los del contenedor"
echo "PID 1 es el entrypoint del contenedor, no systemd"
echo ""
echo "El kernel mantiene dos PIDs para cada proceso:"
echo "  - PID dentro del namespace (lo que vemos con ps)"
echo "  - PID en el host (invisible desde aqui)"
'
```

### Paso 2.3: UTS namespace — hostname aislado

```bash
docker compose exec debian-dev bash -c '
echo "=== UTS namespace: hostname del contenedor ==="
hostname
echo ""
echo "Este hostname es diferente al del host"
echo "Docker lo configura via el UTS namespace"
echo ""
echo "=== /proc/sys/kernel/hostname ==="
cat /proc/sys/kernel/hostname
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== AlmaLinux: hostname ==="
hostname
echo ""
echo "Cada contenedor tiene su propio hostname (UTS namespace)"
'
```

### Paso 2.4: Network namespace — red aislada

```bash
docker compose exec debian-dev bash -c '
echo "=== Network namespace: interfaces de red ==="
ip addr show
echo ""
echo "El contenedor tiene su propia interfaz eth0"
echo "con su propia IP, separada del host"
echo ""
echo "=== Tabla de rutas ==="
ip route
echo ""
echo "El gateway es el bridge de Docker (docker0)"
echo "Docker creo un par veth para conectar el contenedor"
'
```

### Paso 2.5: Mount namespace — filesystem aislado

```bash
docker compose exec debian-dev bash -c '
echo "=== Mount namespace: puntos de montaje ==="
mount | head -10
echo "..."
echo ""
echo "El contenedor tiene sus propios mounts:"
echo "  - / es el rootfs de la imagen"
echo "  - /proc, /sys montados independientemente"
echo "  - /etc/hostname, /etc/resolv.conf son bind mounts"
echo ""
echo "=== Verificar bind mounts de Docker ==="
mount | grep -E "(hostname|resolv|hosts)"
'
```

### Paso 2.6: Cgroup namespace

```bash
docker compose exec debian-dev bash -c '
echo "=== Cgroup namespace: vista local ==="
cat /proc/self/cgroup
echo ""
echo "Desde dentro del contenedor, el cgroup se ve como /"
echo "El contenedor no puede ver los cgroups del host"
echo "ni los de otros contenedores"
echo ""
echo "En el host, este proceso estaria en algo como:"
echo "  0::/system.slice/docker-<hash>.scope"
'
```

---

## Parte 3 — Namespaces y contenedores

### Objetivo

Conectar los namespaces con el funcionamiento de Docker.

### Paso 3.1: Docker usa namespaces

```bash
docker compose exec debian-dev bash -c '
echo "=== Que namespaces usa Docker ==="
echo ""
echo "Docker crea estos namespaces para cada contenedor:"
echo "  PID     — arbol de PIDs propio (PID 1 = entrypoint)"
echo "  Mount   — filesystem propio (imagen + volumes)"
echo "  Network — interfaces de red propias (eth0 en bridge)"
echo "  UTS     — hostname propio"
echo "  IPC     — semaforos/shared memory aislados"
echo "  Cgroup  — vista local de cgroups"
echo ""
echo "Opcional (rootless mode):"
echo "  User    — UIDs/GIDs mapeados (root solo dentro)"
echo ""
echo "Reciente (kernel 5.6+):"
echo "  Time    — relojes aislados (poco usado)"
'
```

### Paso 3.2: nsenter — entrar desde fuera

```bash
docker compose exec debian-dev bash -c '
echo "=== nsenter: entrar a namespaces de otro proceso ==="
echo ""
echo "Desde el HOST se puede entrar al namespace del contenedor:"
echo "  CPID=\$(docker inspect --format {{.State.Pid}} mi-container)"
echo "  sudo nsenter -t \$CPID --all -- /bin/sh"
echo ""
echo "Esto es como docker exec pero a nivel mas bajo"
echo ""
echo "Flags de nsenter:"
echo "  --pid    — PID namespace"
echo "  --net    — Network namespace (util para debug de red)"
echo "  --mount  — Mount namespace"
echo "  --uts    — UTS namespace"
echo "  --all    — todos los namespaces"
echo ""
echo "Caso practico: debug de red del contenedor desde el host:"
echo "  sudo nsenter -t \$CPID --net ss -tlnp"
'
```

### Paso 3.3: unshare — crear namespaces (conceptual)

```bash
docker compose exec debian-dev bash -c '
echo "=== unshare: crear namespaces nuevos ==="
echo ""
echo "unshare crea namespaces nuevos y ejecuta un comando:"
echo "  sudo unshare --pid --fork --mount-proc bash"
echo "  (crea un PID namespace — bash es PID 1)"
echo ""
echo "  sudo unshare --uts bash"
echo "  (crea un UTS namespace — hostname aislado)"
echo ""
echo "  sudo unshare --net bash"
echo "  (crea un network namespace — sin red)"
echo ""
echo "Nota: unshare requiere privilegios que un contenedor"
echo "normal no tiene. Se usa en el HOST o en contenedores"
echo "privilegiados."
echo ""
echo "Docker internamente usa clone() con flags de namespace"
echo "que es la syscall equivalente a unshare pero para fork."
'
```

### Paso 3.4: Verificar aislamiento entre contenedores

```bash
docker compose exec debian-dev bash -c '
echo "=== Debian: namespaces ==="
echo "PID ns:  $(readlink /proc/self/ns/pid)"
echo "Net ns:  $(readlink /proc/self/ns/net)"
echo "UTS ns:  $(readlink /proc/self/ns/uts)"
echo "Mnt ns:  $(readlink /proc/self/ns/mnt)"
echo "Hostname: $(hostname)"
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== AlmaLinux: namespaces ==="
echo "PID ns:  $(readlink /proc/self/ns/pid)"
echo "Net ns:  $(readlink /proc/self/ns/net)"
echo "UTS ns:  $(readlink /proc/self/ns/uts)"
echo "Mnt ns:  $(readlink /proc/self/ns/mnt)"
echo "Hostname: $(hostname)"
'
```

Predice: los inodes de los namespaces seran iguales o diferentes
entre los dos contenedores?

Los namespaces deben tener inodes **diferentes** — cada
contenedor tiene sus propios namespaces. Estan aislados entre si.

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Los **namespaces** aislan la **visibilidad** de recursos: que
   PIDs, interfaces de red, mounts, y hostname ve un proceso.
   Son la base de los contenedores.

2. `/proc/[pid]/ns/` expone los namespaces como inodes. Dos
   procesos con el mismo inode comparten ese namespace.

3. Linux tiene **8 tipos** de namespace: mount, UTS, IPC, PID,
   network, user, cgroup, time. Docker usa todos excepto time
   y opcionalmente user.

4. **nsenter** permite entrar a los namespaces de un proceso
   existente (util para debug). **unshare** crea namespaces
   nuevos (requiere privilegios).

5. Cada contenedor Docker tiene namespaces **propios y separados**
   de otros contenedores. Los inodes de /proc/self/ns/ son
   diferentes entre contenedores.

6. Los namespaces controlan que **ve** el proceso. Los cgroups
   (siguiente topico) controlan cuanto puede **usar**.
