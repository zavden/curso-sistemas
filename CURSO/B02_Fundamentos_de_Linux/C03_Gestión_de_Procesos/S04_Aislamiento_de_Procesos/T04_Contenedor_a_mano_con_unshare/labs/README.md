# Lab — Contenedor a mano con unshare

## Objetivo

Entender como se construye un contenedor desde cero combinando
namespaces (unshare) + filesystem raiz (chroot/pivot_root) +
limites (cgroups). Comparar el resultado con lo que Docker hace
automaticamente.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

Nota: crear namespaces con unshare requiere privilegios que un
contenedor normal no tiene. Este lab se enfoca en entender el
**proceso conceptual** y observar el aislamiento ya existente
en nuestros contenedores.

---

## Parte 1 — unshare y sus flags

### Objetivo

Entender que hace cada flag de unshare y que namespace crea.

### Paso 1.1: El comando completo

```bash
docker compose exec debian-dev bash -c '
echo "=== Comando para crear un contenedor a mano ==="
echo ""
echo "sudo unshare \\"
echo "    --pid \\          # nuevo PID namespace"
echo "    --fork \\         # fork para que el hijo sea PID 1"
echo "    --mount \\        # nuevo mount namespace"
echo "    --uts \\          # nuevo UTS namespace (hostname)"
echo "    --ipc \\          # nuevo IPC namespace"
echo "    --net \\          # nuevo network namespace"
echo "    --mount-proc=rootfs/proc \\"
echo "    chroot rootfs \\"
echo "    /bin/sh"
echo ""
echo "Este comando es funcionalmente equivalente a:"
echo "  docker run -it alpine sh"
'
```

### Paso 1.2: Desglose de cada flag

```bash
docker compose exec debian-dev bash -c '
echo "=== Flag por flag ==="
echo ""
echo "--pid"
echo "  Crea un nuevo PID namespace"
echo "  El primer proceso dentro es PID 1"
echo "  Solo ve sus propios procesos"
echo ""
echo "--fork"
echo "  Necesario con --pid"
echo "  unshare crea el namespace pero el proceso padre"
echo "  NO entra como PID 1 — el hijo si"
echo "  Sin --fork, el shell seria PID != 1"
echo ""
echo "--mount"
echo "  Nuevo mount namespace"
echo "  Montar/desmontar no afecta al host"
echo ""
echo "--uts"
echo "  Nuevo UTS namespace"
echo "  hostname aislado"
echo ""
echo "--ipc"
echo "  Nuevo IPC namespace"
echo "  Semaforos y shared memory aislados"
echo ""
echo "--net"
echo "  Nuevo network namespace"
echo "  Red completamente aislada (vacia — sin interfaces)"
echo ""
echo "--mount-proc=..."
echo "  Monta /proc dentro del rootfs"
echo "  Necesario para que ps, top, etc. funcionen"
echo "  Sin esto, ps mostraria los procesos del host"
'
```

### Paso 1.3: Por que --fork es necesario

```bash
docker compose exec debian-dev bash -c '
echo "=== --fork con --pid ==="
echo ""
echo "Sin --fork:"
echo "  unshare --pid sh"
echo "  sh esta en el PID namespace PADRE"
echo "  (el namespace nuevo no tiene PID 1)"
echo "  ps aux muestra los procesos del host"
echo "  Es un bug comun al usar unshare"
echo ""
echo "Con --fork:"
echo "  unshare --pid --fork sh"
echo "  unshare hace fork()"
echo "  El hijo entra al nuevo PID namespace como PID 1"
echo "  ps aux solo muestra procesos del namespace"
echo ""
echo "Docker no tiene este problema:"
echo "  clone() con CLONE_NEWPID crea el namespace Y"
echo "  el hijo es automaticamente PID 1"
'
```

---

## Parte 2 — Integracion

### Objetivo

Entender como las piezas se combinan para crear un contenedor.

### Paso 2.1: Las 3 piezas de un contenedor

```bash
docker compose exec debian-dev bash -c '
echo "=== Un contenedor = 3 piezas ==="
echo ""
echo "1. NAMESPACES — que ve el proceso"
echo "   PID:     solo sus propios procesos"
echo "   Mount:   su propio filesystem"
echo "   Network: su propia red"
echo "   UTS:     su propio hostname"
echo "   IPC:     su propio IPC"
echo "   User:    sus propios UIDs (opcional, rootless)"
echo ""
echo "2. ROOTFS — donde vive el proceso"
echo "   El filesystem raiz de la imagen"
echo "   Montado via pivot_root (Docker) o chroot (manual)"
echo "   Contiene /bin, /lib, /etc de la distribucion base"
echo ""
echo "3. CGROUPS — cuanto puede usar el proceso"
echo "   memory.max — limite de RAM"
echo "   cpu.max    — limite de CPU"
echo "   pids.max   — limite de procesos"
echo "   io.max     — limite de I/O"
'
```

### Paso 2.2: Verificar las 3 piezas en nuestro contenedor

```bash
docker compose exec debian-dev bash -c '
echo "=== 1. NAMESPACES de este contenedor ==="
echo "PID ns:  $(readlink /proc/self/ns/pid)"
echo "Mnt ns:  $(readlink /proc/self/ns/mnt)"
echo "Net ns:  $(readlink /proc/self/ns/net)"
echo "UTS ns:  $(readlink /proc/self/ns/uts)"
echo "IPC ns:  $(readlink /proc/self/ns/ipc)"

echo ""
echo "=== 2. ROOTFS de este contenedor ==="
echo "Distribucion: $(cat /etc/os-release | grep PRETTY_NAME | cut -d= -f2)"
echo "/ contiene:   $(ls / | tr "\n" " ")"

echo ""
echo "=== 3. CGROUPS de este contenedor ==="
echo "Cgroup:      $(cat /proc/self/cgroup)"
echo "memory.max:  $(cat /sys/fs/cgroup/memory.max 2>/dev/null || echo "N/A")"
echo "pids.max:    $(cat /sys/fs/cgroup/pids.max 2>/dev/null || echo "N/A")"
echo "cpu.max:     $(cat /sys/fs/cgroup/cpu.max 2>/dev/null || echo "N/A")"
'
```

### Paso 2.3: Resultado del aislamiento

```bash
docker compose exec debian-dev bash -c '
echo "=== Que esta aislado en este contenedor ==="
echo ""
echo "--- PIDs (PID namespace) ---"
echo "Procesos visibles: $(ps -e --no-headers | wc -l)"
echo "PID 1 es: $(ps -p 1 -o cmd --no-headers)"
echo "(en el host habria cientos)"

echo ""
echo "--- Red (network namespace) ---"
echo "Interfaces:"
ip -brief addr
echo "(eth0 es un veth creado por Docker)"

echo ""
echo "--- Hostname (UTS namespace) ---"
echo "Hostname: $(hostname)"
echo "(diferente al hostname del host)"

echo ""
echo "--- Filesystem (mount namespace + rootfs) ---"
echo "OS: $(cat /etc/os-release | grep PRETTY_NAME | cut -d= -f2)"
echo "No hay acceso al filesystem del host"
'
```

---

## Parte 3 — Comparacion con Docker

### Objetivo

Entender que hace Docker que un unshare manual no hace.

### Paso 3.1: Lo que falta en un contenedor manual

```bash
docker compose exec debian-dev bash -c '
echo "=== Contenedor manual vs Docker ==="
printf "%-22s %-15s %-15s\n" "Aspecto" "Manual" "Docker"
printf "%-22s %-15s %-15s\n" "--------------------" "-------------" "-------------"
printf "%-22s %-15s %-15s\n" "PID aislado" "Si" "Si"
printf "%-22s %-15s %-15s\n" "Hostname aislado" "Si" "Si"
printf "%-22s %-15s %-15s\n" "Filesystem aislado" "chroot" "pivot_root"
printf "%-22s %-15s %-15s\n" "Red" "Vacia" "Configurada"
printf "%-22s %-15s %-15s\n" "Limites recursos" "No" "Si (cgroups)"
printf "%-22s %-15s %-15s\n" "Senales PID 1" "Basica" "tini/--init"
printf "%-22s %-15s %-15s\n" "Seguridad" "chroot escape" "seccomp+caps"
printf "%-22s %-15s %-15s\n" "User namespace" "No" "Opcional"
printf "%-22s %-15s %-15s\n" "Imagenes en capas" "No" "overlay2"
printf "%-22s %-15s %-15s\n" "Volumenes" "No" "bind+volumes"
printf "%-22s %-15s %-15s\n" "Networking" "Manual" "bridge+NAT"
'
```

### Paso 3.2: Como Docker configura la red

```bash
docker compose exec debian-dev bash -c '
echo "=== Docker y la red ==="
echo ""
echo "Un contenedor manual con --net tiene red VACIA:"
echo "  Solo loopback (lo), y esta DOWN"
echo "  Sin rutas, sin conectividad"
echo ""
echo "Docker configura automaticamente:"
echo "  1. Crea un par veth (virtual ethernet)"
echo "  2. Un extremo en el contenedor (eth0)"
echo "  3. Otro extremo en el host, conectado a docker0 (bridge)"
echo "  4. Asigna IP al contenedor via DHCP/static"
echo "  5. Configura ruta default via el bridge"
echo "  6. Configura NAT en el host (iptables MASQUERADE)"
echo ""
echo "=== Verificar en este contenedor ==="
echo "IP: $(ip -4 addr show eth0 2>/dev/null | grep inet | awk "{print \$2}")"
echo "Gateway: $(ip route | grep default | awk "{print \$3}")"
echo "DNS: $(cat /etc/resolv.conf | grep nameserver | head -1)"
'
```

### Paso 3.3: Capas de seguridad de Docker

```bash
docker compose exec debian-dev bash -c '
echo "=== Seguridad: Docker va mas alla de namespaces ==="
echo ""
echo "1. seccomp (Secure Computing)"
echo "   Filtra syscalls peligrosas"
echo "   ~44 syscalls bloqueadas por defecto"
echo "   (ej: reboot, mount, swapon, clock_settime)"
echo ""
echo "2. Capabilities"
echo "   Docker quita la mayoria de capabilities"
echo "   Por defecto solo tiene ~14 de ~41 capabilities"
echo "   (sin CAP_SYS_ADMIN, CAP_NET_ADMIN, etc.)"
echo ""
echo "3. AppArmor / SELinux"
echo "   Perfil de AppArmor por defecto en Debian/Ubuntu"
echo "   SELinux labels en RHEL"
echo ""
echo "4. Read-only layers"
echo "   Las capas de la imagen son read-only"
echo "   Solo la capa de escritura (container layer) es modificable"
echo ""
echo "=== Capabilities de este contenedor ==="
cat /proc/self/status | grep -i cap
'
```

### Paso 3.4: Panorama completo

```bash
docker compose exec debian-dev bash -c '
echo "=== Resumen de la seccion S04 ==="
echo ""
echo "T01 Namespaces"
echo "    Que VE el proceso (aislamiento de visibilidad)"
echo "    8 tipos: PID, mount, net, UTS, IPC, user, cgroup, time"
echo ""
echo "T02 cgroups v2"
echo "    Cuanto puede USAR (limitacion de recursos)"
echo "    Controladores: memory, cpu, pids, io"
echo ""
echo "T03 chroot y pivot_root"
echo "    Donde VIVE el proceso (filesystem raiz)"
echo "    chroot es inseguro, pivot_root es lo correcto"
echo ""
echo "T04 Este topico"
echo "    INTEGRACION: contenedor = namespaces + rootfs + cgroups"
echo "    Docker automatiza todo esto y agrega seguridad"
echo ""
echo "=== Conclusion ==="
echo "Un contenedor NO es una VM"
echo "Un contenedor ES un proceso Linux con:"
echo "  namespaces (aislamiento)"
echo "  cgroups (limites)"
echo "  rootfs (filesystem)"
echo "  seccomp + capabilities (seguridad)"
echo ""
echo "No hay magia — son features del kernel combinadas"
echo "En B06/C09 implementaremos esto en C con clone() y cgroups"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Un contenedor se construye con **3 piezas**: namespaces
   (aislamiento de visibilidad), rootfs (filesystem propio), y
   cgroups (limites de recursos).

2. `unshare --pid --fork --mount --uts --ipc --net` crea todos
   los namespaces necesarios. `--fork` es obligatorio con `--pid`
   para que el hijo sea PID 1.

3. Docker agrega sobre el contenedor basico: **red configurada**
   (veth+bridge+NAT), **pivot_root** (en lugar de chroot),
   **seccomp**, **capabilities**, e **imagenes en capas**.

4. Un contenedor manual con `--net` tiene red **vacia** (solo lo,
   sin rutas). Docker automatiza la configuracion de red.

5. Un contenedor **no es una VM**: es un proceso Linux normal
   con namespaces, cgroups, y un rootfs diferente. No hay
   hypervisor ni kernel separado.

6. Las capas de seguridad de Docker (seccomp, capabilities,
   AppArmor/SELinux) van mas alla de lo que namespaces y cgroups
   proporcionan.
