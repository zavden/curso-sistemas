# T01 — Namespaces

## Errata y aclaraciones sobre el material original

### Imprecisión: "completaron alrededor de 2016"

README.md línea 23 dice que los namespaces se completaron con cgroup namespace
en 2016. El time namespace se añadió en kernel 5.6 (2020), así que la
"completación" fue en **2020**, no 2016.

### Imprecisión: procesos con & y TTY=?

Labs Paso 1.4 no está presente en este tópico, pero el concepto "procesos
background tienen TTY=?" aparece implícitamente. Como se aclaró en T03_Daemons:
un proceso con `&` desde un shell interactivo **SÍ tiene TTY** (`pts/0`). Solo
`setsid`, namespaces, o daemonización eliminan la TTY.

### Aclaración: --fork con --pid

El README explica por qué `--fork` es necesario con `--pid`, pero de forma
incompleta. El detalle preciso:

```
unshare --pid bash  (SIN --fork):
  unshare crea un nuevo PID namespace
  unshare exec() → bash (reemplaza a unshare)
  bash está en el namespace PADRE (no entró al nuevo)
  Los HIJOS de bash estarán en el nuevo namespace
  Pero no hay PID 1 en el nuevo namespace → los hijos no tienen init
  → cuando un hijo muere, nadie hace wait() → problemas

unshare --pid --fork bash  (CON --fork):
  unshare crea un nuevo PID namespace
  unshare fork() → hijo entra al nuevo namespace como PID 1
  hijo exec() → bash
  bash ES PID 1 del nuevo namespace
  Todo funciona correctamente
```

### Aclaración: tres syscalls para namespaces

El README menciona `unshare` y `nsenter` como comandos, pero no explica las
syscalls subyacentes:

| Syscall | Comando | Qué hace |
|---|---|---|
| `clone()` | — | Fork creando hijo en nuevos namespaces (lo que usa Docker) |
| `unshare()` | `unshare` | El proceso actual crea y entra en nuevos namespaces |
| `setns()` | `nsenter` | El proceso actual entra en namespaces **existentes** |

Docker usa `clone()` porque crea un nuevo proceso (el init del contenedor)
directamente en los nuevos namespaces. `unshare()` es para el caso donde
quieres que el proceso actual se mueva.

---

## Qué son los namespaces

Los namespaces son una funcionalidad del **kernel de Linux** que permite aislar
recursos del sistema para que un grupo de procesos vea su propia instancia de
ese recurso, separada del resto del sistema:

```
Sin namespaces:
  Todos los procesos ven los mismos PIDs, la misma red,
  el mismo hostname, los mismos mount points.

Con namespaces:
  Un grupo de procesos ve sus propios PIDs (empezando en 1),
  su propia interfaz de red, su propio hostname, sus propios mounts.
  Desde dentro, parece un sistema independiente.
```

Los namespaces son **la base de los contenedores**. Docker, Podman, LXC y
cualquier runtime de contenedores usan namespaces internamente. Existen en
Linux desde 2002 (mount namespace) y se completaron en 2020 (time namespace).

> **Namespaces vs cgroups**: los namespaces controlan qué **ve** el proceso
> (visibilidad). Los cgroups (siguiente tópico) controlan cuánto puede **usar**
> (recursos). Juntos, forman el aislamiento de un contenedor.

## Los 8 tipos de namespace

| Namespace | Flag | Aísla | Kernel |
|---|---|---|---|
| Mount | `CLONE_NEWNS` | Puntos de montaje | 2.4.19 (2002) |
| UTS | `CLONE_NEWUTS` | Hostname y domainname | 2.6.19 (2006) |
| IPC | `CLONE_NEWIPC` | Semáforos, colas, shared memory (System V) | 2.6.19 (2006) |
| PID | `CLONE_NEWPID` | Árbol de PIDs | 2.6.24 (2008) |
| Network | `CLONE_NEWNET` | Interfaces de red, rutas, iptables, sockets | 2.6.29 (2009) |
| User | `CLONE_NEWUSER` | UIDs/GIDs (root dentro sin serlo fuera) | 3.8 (2013) |
| Cgroup | `CLONE_NEWCGROUP` | Vista del árbol de cgroups | 4.6 (2016) |
| Time | `CLONE_NEWTIME` | CLOCK_MONOTONIC y CLOCK_BOOTTIME | 5.6 (2020) |

### Mount namespace (CLONE_NEWNS)

El primer namespace implementado (por eso su flag es genérica: `NEWNS`):

```bash
# Cada mount namespace tiene su propio árbol de puntos de montaje
# Un proceso puede montar/desmontar sin afectar al host

# En contenedores:
# - El contenedor tiene su propio rootfs
# - /proc, /sys, /dev montados independientemente
# - Montar un volumen solo afecta al contenedor
```

### PID namespace (CLONE_NEWPID)

```bash
# Cada PID namespace tiene su propia numeración
# El primer proceso dentro es PID 1

# Desde DENTRO del contenedor:
#   PID 1 = entrypoint del contenedor
#   PID 2,3... = sus hijos

# Desde FUERA (el host):
#   El mismo proceso tiene un PID diferente (ej: 45230)

# Un proceso tiene DOS PIDs simultáneamente:
#   - PID en su namespace (lo que ve ps dentro)
#   - PID en el namespace padre (lo que ve ps en el host)
```

### Network namespace (CLONE_NEWNET)

```bash
# Cada network namespace tiene:
# - Sus propias interfaces de red
# - Su propia tabla de rutas
# - Sus propias reglas de iptables/nftables
# - Sus propios sockets (puede usar puerto 80 sin conflicto)

# Un namespace nuevo empieza VACÍO:
# - Solo tiene loopback (lo), y está DOWN
# - No tiene rutas ni conectividad
# - Docker configura: veth pair + bridge (docker0)

# Docker crea un par veth (virtual ethernet):
# - Un extremo en el namespace del contenedor (eth0)
# - El otro en el host, conectado al bridge docker0
```

### UTS namespace (CLONE_NEWUTS)

```bash
# UTS = "Unix Time-Sharing" (nombre histórico)
# Aísla hostname y domainname

# Cada contenedor tiene su propio hostname:
docker run --hostname my-server alpine hostname
# my-server (sin afectar al host)
```

### IPC namespace (CLONE_NEWIPC)

```bash
# Aísla comunicación entre procesos de System V:
# - Semáforos
# - Colas de mensajes
# - Shared memory segments

# Previene que contenedores interfieran entre sí vía IPC
```

### User namespace (CLONE_NEWUSER)

```bash
# Permite mapear UIDs/GIDs:
# - Un proceso puede ser root (UID 0) DENTRO del namespace
# - Pero ser UID 100000 (sin privilegios) FUERA

# Base de los "rootless containers":
# - El contenedor opera como root internamente
# - Si escapa del contenedor, es un usuario sin privilegios en el host

# Docker sin rootless: procesos del contenedor SON root en el host
# Docker con rootless / Podman: usan user namespaces para aislar
```

### Cgroup namespace (CLONE_NEWCGROUP)

```bash
# Aísla la vista del árbol de cgroups
# Un proceso dentro del namespace ve su cgroup como raíz

cat /proc/self/cgroup
# En el host:      0::/user.slice/user-1000.slice/session-1.scope
# En contenedor:   0::/    ← ve su cgroup como raíz
```

### Time namespace (CLONE_NEWTIME)

```bash
# El más reciente — permite que un proceso vea un reloj diferente
# Solo afecta a CLOCK_MONOTONIC y CLOCK_BOOTTIME
# NO afecta a CLOCK_REALTIME (la hora del día)
# Útil para checkpoint/restore de contenedores
```

## Ver namespaces activos

### /proc/[pid]/ns/

```bash
# Cada proceso expone sus namespaces como symlinks
ls -la /proc/$$/ns/
# lrwxrwxrwx ... cgroup -> 'cgroup:[4026531835]'
# lrwxrwxrwx ... ipc    -> 'ipc:[4026531839]'
# lrwxrwxrwx ... mnt    -> 'mnt:[4026531841]'
# lrwxrwxrwx ... net    -> 'net:[4026531840]'
# lrwxrwxrwx ... pid    -> 'pid:[4026531836]'
# lrwxrwxrwx ... user   -> 'user:[4026531837]'
# lrwxrwxrwx ... uts    -> 'uts:[4026531838]'

# El número entre corchetes es el INODE del namespace
# Mismo inode = mismo namespace (compartido)
# Diferente inode = namespaces aislados
```

### lsns

```bash
# Listar todos los namespaces del sistema
lsns
# NS         TYPE   NPROCS PID USER COMMAND
# 4026531835 cgroup    150   1 root /usr/lib/systemd/systemd
# 4026531836 pid       150   1 root /usr/lib/systemd/systemd
# ...
# 4026532189 mnt         1 842 root /usr/sbin/sshd -D

# Filtrar por tipo
lsns -t net
lsns -t pid

# De un proceso específico
lsns -p $$
```

## nsenter — Entrar en un namespace existente

`nsenter` ejecuta un comando en los namespaces de otro proceso (internamente
usa la syscall `setns()`):

```bash
# Entrar en todos los namespaces de un proceso
sudo nsenter -t <PID> --all

# Entrar en namespaces específicos
sudo nsenter -t <PID> --net          # solo network
sudo nsenter -t <PID> --pid --mount  # PID y mount

# Ejemplo: entrar en un contenedor Docker desde el host
CPID=$(docker inspect --format '{{.State.Pid}}' mi-container)
sudo nsenter -t $CPID --all -- /bin/sh
# Ahora estás "dentro" del contenedor sin usar docker exec
```

### Flags de nsenter

| Flag | Namespace |
|---|---|
| `--mount` / `-m` | Mount |
| `--uts` / `-u` | UTS (hostname) |
| `--ipc` / `-i` | IPC |
| `--net` / `-n` | Network |
| `--pid` / `-p` | PID |
| `--user` / `-U` | User |
| `--cgroup` / `-C` | Cgroup |
| `--time` / `-T` | Time |
| `--all` | Todos |

```bash
# Caso práctico: debugging de red de un contenedor
CPID=$(docker inspect --format '{{.State.Pid}}' mi-container)

# Entrar solo al network namespace
sudo nsenter -t $CPID --net

# Comandos de red ven la red del contenedor
ip addr          # interfaces del contenedor
ip route         # rutas del contenedor
ss -tlnp         # puertos escuchando

# Pero el filesystem sigue siendo el del host
# (no entramos al mount namespace)
```

## unshare — Crear un namespace nuevo

`unshare` crea namespaces nuevos (internamente usa la syscall `unshare()`):

```bash
# UTS namespace (hostname aislado)
sudo unshare --uts bash
hostname my-isolated-host
hostname     # my-isolated-host
exit
hostname     # mi-laptop (sin cambios)
```

```bash
# PID namespace
sudo unshare --pid --fork --mount-proc bash
ps aux
# PID USER  ... COMMAND
#   1 root  ... bash      ← eres PID 1 en este namespace
#   2 root  ... ps aux
# Solo ves los procesos dentro del namespace
exit
```

### Flags importantes

| Flag | Qué hace |
|---|---|
| `--pid` | Nuevo PID namespace |
| `--fork` | Fork antes de ejecutar (necesario con `--pid`) |
| `--mount-proc` | Montar `/proc` nuevo (para que `ps` funcione) |
| `--mount` | Nuevo mount namespace |
| `--uts` | Nuevo UTS namespace |
| `--net` | Nuevo network namespace |
| `--user` | Nuevo user namespace |
| `--map-root-user` | Mapear UID actual a root dentro del user namespace |

```bash
# ¿Por qué --mount-proc?
# /proc muestra los procesos del PID namespace del kernel que lo montó.
# Sin remontarlo, ps mostraría TODOS los procesos del host.
# --mount-proc = --mount + mount -t proc proc /proc
```

## Namespaces y Docker

Docker usa **todos** los namespaces (excepto time en configuraciones estándar):

```
docker run -it alpine sh

Docker internamente:
  1. clone() con flags:
     CLONE_NEWPID    → PID namespace nuevo
     CLONE_NEWNS     → Mount namespace nuevo
     CLONE_NEWNET    → Network namespace nuevo
     CLONE_NEWUTS    → UTS namespace nuevo
     CLONE_NEWIPC    → IPC namespace nuevo
     CLONE_NEWCGROUP → Cgroup namespace nuevo
     (CLONE_NEWUSER si rootless)

  2. Configura red (veth pair + bridge docker0)
  3. Monta rootfs de la imagen
  4. Monta /proc, /sys, /dev
  5. pivot_root al rootfs
  6. Ejecuta entrypoint
```

```bash
# Verificar aislamiento host vs contenedor
docker run -d --name test alpine sleep 300
CPID=$(docker inspect --format '{{.State.Pid}}' test)

# Comparar inodes de namespaces
echo "=== Host (PID 1) ==="
ls -la /proc/1/ns/ | awk '{print $NF}'

echo "=== Container ==="
ls -la /proc/$CPID/ns/ | awk '{print $NF}'

# Inodes diferentes → aislados
docker rm -f test
```

### ip netns — network namespaces persistentes

```bash
# ip netns crea network namespaces que persisten en /var/run/netns/
sudo ip netns add test-ns
sudo ip netns exec test-ns ip addr   # ejecutar en el namespace
sudo ip netns delete test-ns

# Docker NO usa ip netns — gestiona namespaces vía clone()/setns()
# ip netns es útil para testing y configuración manual de red
```

## Diferencias entre distribuciones

| Aspecto | Debian/Ubuntu | RHEL/AlmaLinux |
|---|---|---|
| User namespaces | Habilitados (puede restringirse) | Habilitados (RHEL 8+) |
| `unshare`, `nsenter`, `lsns` | Paquete `util-linux` | Paquete `util-linux` |
| Restricción unprivileged | `kernel.unprivileged_userns_clone` | Sin restricción extra |

```bash
# Debian/Ubuntu: puede restringir user namespaces para no-root
sysctl kernel.unprivileged_userns_clone
# 1 = permitido (default en mayoría)
# 0 = solo root puede crear user namespaces
```

---

## Labs

> **Nota**: Dentro de un contenedor Docker sin privilegios no se pueden crear
> namespaces nuevos. Estos labs se enfocan en **observar y entender** los
> namespaces existentes.

### Parte 1 — Ver namespaces

#### Paso 1.1: Namespaces del proceso actual

```bash
echo "=== /proc/self/ns/ ==="
ls -la /proc/self/ns/

echo ""
echo "Cada enlace apunta a un namespace identificado por inode"
echo "Mismo inode = mismo namespace (compartido)"
echo "Diferente inode = namespaces aislados"
```

#### Paso 1.2: Comparar namespaces entre procesos

```bash
echo "=== Shell (PID $$) ==="
ls -la /proc/self/ns/ | awk '{print $NF}' | sort

echo ""
echo "=== PID 1 ==="
ls -la /proc/1/ns/ 2>/dev/null | awk '{print $NF}' | sort

echo ""
echo "Si los inodes son iguales → mismos namespaces"
echo "Dentro de un contenedor, todos los procesos"
echo "típicamente comparten los mismos namespaces"
```

#### Paso 1.3: lsns

```bash
echo "=== lsns ==="
lsns 2>/dev/null || echo "(lsns no disponible)"

echo ""
echo "Columnas:"
echo "  NS      — inode del namespace"
echo "  TYPE    — tipo (pid, net, mnt, uts, ipc, user, cgroup)"
echo "  NPROCS  — procesos en este namespace"
echo "  PID     — PID del proceso líder"
```

#### Paso 1.4: Filtrar por tipo

```bash
for TYPE in pid net mnt uts; do
    echo "=== $TYPE namespaces ==="
    lsns -t $TYPE 2>/dev/null || echo "(no disponible)"
    echo ""
done
```

### Parte 2 — Observar aislamiento

#### Paso 2.1: PID namespace

```bash
echo "=== PID namespace: solo vemos nuestros procesos ==="
echo "Total visible: $(ps -e --no-headers | wc -l)"
echo ""
ps -eo pid,ppid,cmd | head -10
echo ""
echo "En el host habría cientos de procesos"
echo "PID 1 aquí es el entrypoint, no systemd"
```

#### Paso 2.2: UTS namespace (hostname)

```bash
echo "=== Hostname del contenedor ==="
hostname
echo ""
echo "=== /proc/sys/kernel/hostname ==="
cat /proc/sys/kernel/hostname
echo ""
echo "Cada contenedor tiene su propio hostname (UTS namespace)"
```

#### Paso 2.3: Network namespace

```bash
echo "=== Interfaces de red ==="
ip addr show 2>/dev/null || ifconfig 2>/dev/null

echo ""
echo "=== Tabla de rutas ==="
ip route 2>/dev/null

echo ""
echo "El contenedor tiene su propia eth0 con IP propia"
echo "Docker creó un par veth para conectar al bridge"
```

#### Paso 2.4: Mount namespace

```bash
echo "=== Puntos de montaje ==="
mount | head -10
echo "..."

echo ""
echo "=== Bind mounts de Docker ==="
mount | grep -E '(hostname|resolv|hosts)'
echo ""
echo "Docker inyecta hostname, resolv.conf y hosts como bind mounts"
```

#### Paso 2.5: Cgroup namespace

```bash
echo "=== Vista de cgroups desde dentro ==="
cat /proc/self/cgroup
echo ""
echo "Se ve como '/' — el contenedor no sabe su posición"
echo "real en el árbol de cgroups del host"
```

### Parte 3 — Herramientas

#### Paso 3.1: nsenter y unshare (conceptual)

```bash
echo "=== nsenter: entrar a namespaces existentes ==="
echo "Desde el HOST:"
echo "  CPID=\$(docker inspect --format '{{.State.Pid}}' container)"
echo "  sudo nsenter -t \$CPID --all -- /bin/sh"
echo ""
echo "  --net solo: debug de red sin tocar filesystem"
echo "  sudo nsenter -t \$CPID --net ss -tlnp"
echo ""
echo "=== unshare: crear namespaces nuevos ==="
echo "  sudo unshare --pid --fork --mount-proc bash"
echo "  sudo unshare --uts bash"
echo "  sudo unshare --net bash"
echo ""
echo "Nota: requiere privilegios (host o contenedor privilegiado)"
```

---

## Ejercicios

### Ejercicio 1 — Ver tus namespaces

```bash
# a) Listar los namespaces de tu shell
ls -la /proc/$$/ns/

# b) ¿Cuántos tipos de namespace tienes?
ls /proc/$$/ns/ | wc -l

# c) Extraer solo los inodes
ls -la /proc/$$/ns/ | awk -F'[' '{print $2}' | tr -d ']'
```

<details><summary>Predicción</summary>

a) Se ven symlinks como `pid -> 'pid:[4026531836]'`. Cada uno apunta al
namespace de ese tipo con su inode.

b) Típicamente **7 u 8** tipos: cgroup, ipc, mnt, net, pid, pid_for_children,
user, uts. En kernels 5.6+, también `time` y `time_for_children`. Los
`_for_children` indican el namespace que heredarán los hijos (puede diferir
del propio tras un `unshare`).

c) Los inodes son números como `4026531836`. Son únicos por namespace. Dos
procesos con el mismo inode para un tipo dado están en el **mismo** namespace
de ese tipo.

</details>

### Ejercicio 2 — ¿Mismo namespace o aislado?

```bash
# a) Comparar tu shell con PID 1
echo "Shell PID ns:  $(readlink /proc/$$/ns/pid)"
echo "PID 1 PID ns:  $(readlink /proc/1/ns/pid)"

# b) Comparar network
echo ""
echo "Shell NET ns:  $(readlink /proc/$$/ns/net)"
echo "PID 1 NET ns:  $(readlink /proc/1/ns/net)"

# c) ¿Son iguales o diferentes?
SHELL_PID_NS=$(readlink /proc/$$/ns/pid)
PID1_PID_NS=$(readlink /proc/1/ns/pid)
if [ "$SHELL_PID_NS" = "$PID1_PID_NS" ]; then
    echo "Mismo PID namespace"
else
    echo "PID namespaces DIFERENTES"
fi
```

<details><summary>Predicción</summary>

a-b) Dentro de un **contenedor**: los inodes son **iguales**. Tu shell y PID 1
del contenedor están en los mismos namespaces — Docker creó un set de
namespaces para todo el contenedor.

En el **host**: también iguales, a menos que el shell esté en un contenedor o
namespace personalizado.

c) "Mismo PID namespace". Todos los procesos dentro de un contenedor comparten
los mismos namespaces por defecto. El aislamiento es entre **contenedores**
(cada uno tiene sus propios namespaces), no entre procesos dentro del mismo
contenedor.

Excepción: `docker run --pid=host` comparte el PID namespace con el host.

</details>

### Ejercicio 3 — lsns: inventario de namespaces

```bash
# a) Listar todos los namespaces visibles
lsns 2>/dev/null

# b) ¿Cuántos namespaces de cada tipo?
echo ""
echo "=== Conteo por tipo ==="
lsns --no-headings 2>/dev/null | awk '{print $2}' | sort | uniq -c | sort -rn

# c) ¿Cuántos procesos comparten el namespace de PID?
echo ""
echo "=== PID namespace ==="
lsns -t pid 2>/dev/null
```

<details><summary>Predicción</summary>

a) Dentro de un contenedor: se ven pocos namespaces (uno de cada tipo), todos
compartidos por los procesos del contenedor.

b) Típicamente 1 de cada tipo dentro de un contenedor (todos compartidos).
En el host con contenedores corriendo: múltiples namespaces de net, pid, mnt
(uno extra por cada contenedor).

c) El PID namespace muestra todos los procesos del contenedor compartiendo
un solo namespace. NPROCS indica cuántos procesos hay dentro.

`lsns` solo muestra namespaces visibles desde el namespace actual. Desde
dentro de un contenedor, no puedes ver los namespaces del host ni de otros
contenedores.

</details>

### Ejercicio 4 — PID namespace: aislamiento

```bash
# a) ¿Cuántos procesos ves?
echo "Procesos visibles: $(ps -e --no-headers | wc -l)"

# b) ¿Cuál es PID 1?
echo "PID 1: $(ps -o cmd= -p 1)"

# c) ¿Hay kernel threads visibles?
echo ""
echo "Kernel threads (PPID 2):"
ps -eo pid,ppid,cmd | awk '$2 == 2' | head -3
echo "(¿alguno visible?)"

# d) PID del shell actual
echo ""
echo "Mi PID: $$"
```

<details><summary>Predicción</summary>

a) Pocos procesos (decenas como mucho). En el host habría cientos.

b) PID 1 = el entrypoint del contenedor (no systemd). Esto es por el PID
namespace: el primer proceso del contenedor es PID 1 dentro de su namespace.

c) **Ninguno visible**. Los kernel threads (rcu, kworker, etc.) están en el
PID namespace del host (PID 2 = kthreadd). Desde dentro del contenedor,
PID 2 no es kthreadd — es el segundo proceso del contenedor.

d) Un número mayor que 1. Tu shell fue lanzado como un proceso dentro del
contenedor, recibió un PID secuencial en el PID namespace del contenedor.

</details>

### Ejercicio 5 — UTS namespace: hostname aislado

```bash
# a) ¿Cuál es el hostname?
hostname

# b) ¿De dónde viene?
cat /proc/sys/kernel/hostname

# c) ¿Podemos cambiarlo? (normalmente no, sin privilegios)
hostname test-change 2>&1 || echo "No se puede cambiar (sin privilegios)"

# d) ¿Dónde está el hostname según mount?
mount | grep hostname
```

<details><summary>Predicción</summary>

a) Un hash o el nombre configurado por Docker (por defecto, el container ID
corto de 12 chars).

b) Mismo valor. `/proc/sys/kernel/hostname` refleja el hostname del UTS
namespace actual.

c) Falla con "Permission denied" o similar. En un contenedor sin privilegios,
no se puede cambiar el hostname. Docker lo configura al crear el contenedor
mediante el UTS namespace.

d) Se ve un bind mount: `/etc/hostname` es montado por Docker desde el host.
Docker escribe el hostname deseado en este archivo y lo inyecta en el
contenedor.

El UTS namespace permite que cada contenedor tenga un hostname independiente
sin afectar al host ni a otros contenedores.

</details>

### Ejercicio 6 — Network namespace: red aislada

```bash
# a) Interfaces de red
ip addr show 2>/dev/null

# b) Tabla de rutas
echo ""
ip route 2>/dev/null

# c) ¿Qué puertos están escuchando?
echo ""
ss -tlnp 2>/dev/null || netstat -tlnp 2>/dev/null

# d) ¿Cuál es la IP del contenedor?
echo ""
echo "IP: $(hostname -I 2>/dev/null | awk '{print $1}')"
```

<details><summary>Predicción</summary>

a) Se ven dos interfaces:
- `lo` (loopback, 127.0.0.1)
- `eth0` (la interfaz del contenedor, IP tipo 172.x.x.x)

No se ve la interfaz física del host (eth0/wlan0 del host). Esto es el
aislamiento del network namespace.

b) Ruta default apunta al gateway del bridge Docker (típicamente 172.17.0.1
o similar). Cada contenedor tiene su propia tabla de rutas.

c) Solo los puertos que escuchan procesos **dentro** de este contenedor. Los
puertos del host o de otros contenedores no son visibles.

d) IP privada asignada por Docker (172.x.x.x o red personalizada). Cada
contenedor tiene su propia IP dentro del network namespace.

Docker implementa esto creando un **veth pair**: un extremo en el namespace
del contenedor (eth0), el otro en el host conectado al bridge docker0.

</details>

### Ejercicio 7 — Mount namespace: filesystem aislado

```bash
# a) ¿Cuál es el rootfs?
echo "Root: $(mount | grep ' / ' | head -1)"

# b) Bind mounts inyectados por Docker
echo ""
echo "=== Bind mounts de Docker ==="
mount | grep -E '(hostname|resolv\.conf|hosts)'

# c) ¿/proc es el mismo que el del host?
echo ""
echo "PID máximo visible en /proc:"
ls -d /proc/[0-9]* 2>/dev/null | wc -l

# d) ¿Hay mount points del host visibles?
echo ""
echo "Mount points totales: $(mount | wc -l)"
```

<details><summary>Predicción</summary>

a) El root filesystem es un overlay (overlayfs) — la tecnología de
capas de Docker. El contenedor ve esto como su `/`.

b) Docker inyecta tres archivos como bind mounts:
- `/etc/hostname` — para que `hostname` funcione
- `/etc/resolv.conf` — para DNS
- `/etc/hosts` — para resolución local

Estos son bind mounts desde el host, no parte de la imagen.

c) Pocos PIDs (solo los del contenedor). El `/proc` fue montado dentro del
PID namespace del contenedor, así que solo muestra procesos de este namespace.

d) Solo los mounts del contenedor. Los mounts del host (como `/home`, `/boot`)
no son visibles. Esto es el aislamiento del mount namespace.

</details>

### Ejercicio 8 — Cgroup namespace: vista local

```bash
# a) ¿En qué cgroup estamos?
echo "=== /proc/self/cgroup ==="
cat /proc/self/cgroup

# b) ¿Todos los procesos del contenedor están en el mismo cgroup?
echo ""
echo "=== Cgroup de PID 1 ==="
cat /proc/1/cgroup 2>/dev/null

# c) ¿Podemos ver cgroups del host?
echo ""
echo "=== /sys/fs/cgroup ==="
ls /sys/fs/cgroup/ 2>/dev/null | head -10
```

<details><summary>Predicción</summary>

a) Se ve `0::/` — el contenedor ve su cgroup como la raíz `/`. Desde el
host, este mismo proceso estaría en algo como
`0::/system.slice/docker-<hash>.scope`.

b) Mismo cgroup que nuestro shell (`0::/`). Todos los procesos del contenedor
están en el mismo cgroup (desde la perspectiva del cgroup namespace).

c) Se ve el contenido de `/sys/fs/cgroup` del contenedor. Puede estar limitado
(read-only o parcialmente montado). Los controladores visibles (cpu, memory,
pids) son los que aplican a este contenedor.

El cgroup namespace oculta la posición real del contenedor en el árbol de
cgroups del host. Esto previene que el contenedor manipule o vea los recursos
de otros contenedores.

</details>

### Ejercicio 9 — Comparar namespaces entre shells

```bash
# a) Abrir un nuevo proceso y comparar namespaces
bash -c '
    echo "Shell original:"
    readlink /proc/$PPID/ns/pid
    readlink /proc/$PPID/ns/net

    echo ""
    echo "Nuevo bash:"
    readlink /proc/$$/ns/pid
    readlink /proc/$$/ns/net
'

# b) ¿Comparten los mismos namespaces?
echo ""
echo "PID 1:    $(readlink /proc/1/ns/pid)"
echo "Shell:    $(readlink /proc/$$/ns/pid)"

# c) ¿Podemos ver pid_for_children?
echo ""
echo "pid_for_children: $(readlink /proc/$$/ns/pid_for_children 2>/dev/null || echo 'no disponible')"
```

<details><summary>Predicción</summary>

a) Los inodes son **iguales**. El nuevo bash hereda los namespaces del padre.
Dentro de un contenedor, todos los procesos comparten los mismos namespaces.

b) Iguales. PID 1 y el shell están en el mismo PID namespace. El aislamiento
es entre contenedores, no entre procesos del mismo contenedor.

c) `pid_for_children` muestra el PID namespace que heredarán los hijos de
este proceso. Normalmente es igual al PID namespace propio (`/proc/$$/ns/pid`).
Si el proceso hiciera `unshare(CLONE_NEWPID)`, `pid_for_children` cambiaría
pero `pid` seguiría siendo el original — los nuevos hijos estarían en un
namespace diferente al padre.

</details>

### Ejercicio 10 — ¿Qué namespaces usó Docker?

```bash
# Verificar todos los namespaces activos y razonar sobre Docker

echo "=== Todos los namespaces de este contenedor ==="
for NS in $(ls /proc/self/ns/); do
    INODE=$(readlink /proc/self/ns/$NS)
    echo "  $NS: $INODE"
done

echo ""
echo "=== ¿Cuáles creó Docker? ==="
echo "Responder: ¿cuáles de estos son diferentes a los del host?"
echo ""
echo "Docker crea estos namespaces para cada contenedor:"
echo "  PID     → $(readlink /proc/self/ns/pid)"
echo "  Mount   → $(readlink /proc/self/ns/mnt)"
echo "  Network → $(readlink /proc/self/ns/net)"
echo "  UTS     → $(readlink /proc/self/ns/uts)"
echo "  IPC     → $(readlink /proc/self/ns/ipc)"
echo "  Cgroup  → $(readlink /proc/self/ns/cgroup)"
echo ""
echo "Estos podrían ser los mismos del host (depende de config):"
echo "  User    → $(readlink /proc/self/ns/user)"
echo "  Time    → $(readlink /proc/self/ns/time 2>/dev/null || echo 'no disponible')"
```

<details><summary>Predicción</summary>

Docker crea 6 namespaces propios para cada contenedor: **PID, Mount, Network,
UTS, IPC, Cgroup**. Estos tienen inodes diferentes a los del host.

User namespace: **compartido con el host** en Docker estándar (sin rootless).
Los procesos del contenedor son realmente root en el host. Con Docker rootless
o Podman, el user namespace sería diferente.

Time namespace: generalmente **compartido con el host**. Docker no crea time
namespaces por defecto (kernel 5.6+ requerido, y no hay caso de uso común).

Para verificar, habría que comparar estos inodes con los de un proceso del
host (ej: `ls -la /proc/1/ns/` en el host). Si el inode es diferente →
Docker creó un namespace propio. Si es igual → namespace compartido.

</details>
