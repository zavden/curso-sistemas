# T01 — Namespaces

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
cualquier runtime de contenedores usan namespaces internamente. No son una
tecnología nueva — existen en Linux desde 2002 (mount namespace) y se
completaron alrededor de 2016 (cgroup namespace).

## Los 8 tipos de namespace

| Namespace | Flag | Aísla | Desde kernel |
|---|---|---|---|
| Mount | CLONE_NEWNS | Puntos de montaje (qué filesystems ve el proceso) | 2.4.19 (2002) |
| UTS | CLONE_NEWUTS | Hostname y domainname | 2.6.19 (2006) |
| IPC | CLONE_NEWIPC | Semáforos, colas de mensajes, shared memory de System V | 2.6.19 (2006) |
| PID | CLONE_NEWPID | PIDs (el proceso ve su propio árbol de PIDs) | 2.6.24 (2008) |
| Network | CLONE_NEWNET | Interfaces de red, rutas, iptables, sockets | 2.6.29 (2009) |
| User | CLONE_NEWUSER | UIDs/GIDs (root dentro del namespace sin serlo fuera) | 3.8 (2013) |
| Cgroup | CLONE_NEWCGROUP | Vista del árbol de cgroups | 4.6 (2016) |
| Time | CLONE_NEWTIME | CLOCK_MONOTONIC y CLOCK_BOOTTIME del sistema | 5.6 (2020) |

### Mount namespace (CLONE_NEWNS)

El primer namespace que se implementó (por eso su flag se llama genéricamente
`NEWNS` — "new namespace" — sin especificar "mount"):

```bash
# Cada mount namespace tiene su propio árbol de puntos de montaje
# Un proceso puede montar/desmontar filesystems sin afectar al host

# En contenedores:
# - El contenedor tiene su propio rootfs montado
# - /proc, /sys, /dev son montados independientemente
# - Montar un volumen en un contenedor no aparece en el host
#   (excepto que el runtime lo configure así con bind mounts)
```

### PID namespace (CLONE_NEWPID)

```bash
# Cada PID namespace tiene su propia numeración de PIDs
# El primer proceso dentro del namespace es PID 1

# Desde DENTRO del contenedor:
#   PID 1 = el entrypoint del contenedor
#   PID 2,3... = sus hijos

# Desde FUERA (el host):
#   El mismo proceso tiene un PID diferente (ej: PID 45230)
#   Los PIDs del contenedor no son visibles como tales

# Un proceso tiene dos PIDs:
#   - PID en su namespace (lo que ve ps dentro del contenedor)
#   - PID en el namespace padre (lo que ve ps en el host)
```

### Network namespace (CLONE_NEWNET)

```bash
# Cada network namespace tiene:
# - Sus propias interfaces de red (no comparte eth0 con el host)
# - Su propia tabla de rutas
# - Sus propias reglas de iptables/nftables
# - Sus propios sockets (puede usar el puerto 80 sin conflicto con el host)

# Un network namespace nuevo empieza VACÍO:
# - Solo tiene la interfaz loopback (lo), y está DOWN
# - No tiene rutas, no tiene conectividad
# - Hay que configurar interfaces y rutas (o Docker lo hace por ti)

# Docker crea un par veth (virtual ethernet):
# - Un extremo en el namespace del contenedor (eth0)
# - El otro extremo en el host, conectado a docker0 (bridge)
```

### UTS namespace (CLONE_NEWUTS)

```bash
# UTS = "Unix Time-Sharing" (nombre histórico)
# Aísla hostname y domainname

# Cada contenedor puede tener su propio hostname:
docker run --hostname mi-server alpine hostname
# mi-server

# Sin afectar al host:
hostname
# mi-laptop (sin cambios)
```

### IPC namespace (CLONE_NEWIPC)

```bash
# Aísla los mecanismos de comunicación entre procesos de System V:
# - Semáforos
# - Colas de mensajes
# - Shared memory segments

# Un proceso en un IPC namespace no puede acceder a los semáforos
# o shared memory de otro namespace
# Previene que contenedores interfieran entre sí via IPC
```

### User namespace (CLONE_NEWUSER)

```bash
# Permite mapear UIDs/GIDs:
# - Un proceso puede ser root (UID 0) DENTRO del namespace
# - Pero ser un usuario sin privilegios (UID 100000) FUERA

# Esto es la base de los "rootless containers":
# - El contenedor cree que es root y puede hacer operaciones de root
# - Pero en el host no tiene privilegios reales
# - Si el proceso escapa del contenedor, es un usuario sin privilegios

# Docker sin rootless: los procesos del contenedor SON root en el host
# Docker con rootless / Podman: usan user namespaces para aislar
```

### Cgroup namespace (CLONE_NEWCGROUP)

```bash
# Aísla la vista del árbol de cgroups
# Un proceso dentro del namespace ve su cgroup como la raíz
# No puede ver los cgroups del host ni de otros contenedores

cat /proc/self/cgroup
# En el host:    0::/user.slice/user-1000.slice/session-1.scope
# En contenedor: 0::/                 ← ve su cgroup como raíz
```

### Time namespace (CLONE_NEWTIME)

```bash
# El más reciente — permite que un proceso vea un reloj diferente
# Útil para migración de contenedores (checkpoint/restore)
# y para testing

# Solo afecta a CLOCK_MONOTONIC y CLOCK_BOOTTIME
# NO afecta a CLOCK_REALTIME (la hora del día)
```

## Ver namespaces activos

### lsns

```bash
# Listar todos los namespaces del sistema
lsns
# NS         TYPE   NPROCS   PID USER  COMMAND
# 4026531835 cgroup    150     1 root  /usr/lib/systemd/systemd
# 4026531836 pid       150     1 root  /usr/lib/systemd/systemd
# 4026531837 user      150     1 root  /usr/lib/systemd/systemd
# 4026531838 uts       150     1 root  /usr/lib/systemd/systemd
# 4026531839 ipc       150     1 root  /usr/lib/systemd/systemd
# 4026531840 net       150     1 root  /usr/lib/systemd/systemd
# 4026531841 mnt       140     1 root  /usr/lib/systemd/systemd
# 4026532189 mnt         1   842 root  /usr/sbin/sshd -D

# Si hay contenedores corriendo, aparecen namespaces adicionales

# Filtrar por tipo
lsns -t net
lsns -t pid

# De un proceso específico
lsns -p 1234
```

### /proc/[pid]/ns/

```bash
# Cada proceso expone sus namespaces como symlinks
ls -la /proc/$$/ns/
# lrwxrwxrwx 1 dev dev ... cgroup -> 'cgroup:[4026531835]'
# lrwxrwxrwx 1 dev dev ... ipc -> 'ipc:[4026531839]'
# lrwxrwxrwx 1 dev dev ... mnt -> 'mnt:[4026531841]'
# lrwxrwxrwx 1 dev dev ... net -> 'net:[4026531840]'
# lrwxrwxrwx 1 dev dev ... pid -> 'pid:[4026531836]'
# lrwxrwxrwx 1 dev dev ... user -> 'user:[4026531837]'
# lrwxrwxrwx 1 dev dev ... uts -> 'uts:[4026531838]'

# El número entre corchetes es el inode del namespace
# Dos procesos con el mismo número están en el MISMO namespace
# Dos procesos con números diferentes están AISLADOS en esa dimensión

# Comparar host con contenedor
ls -la /proc/1/ns/net         # namespace del host
ls -la /proc/45230/ns/net     # namespace del contenedor
# Si los inodes son diferentes → redes aisladas
```

## nsenter — Entrar en un namespace existente

`nsenter` permite a un proceso **entrar en los namespaces de otro proceso**.
Es como meterse dentro de un contenedor desde el host:

```bash
# Entrar en todos los namespaces de un proceso
sudo nsenter -t <PID> --all

# Entrar en namespaces específicos
sudo nsenter -t <PID> --net          # solo network namespace
sudo nsenter -t <PID> --pid --mount  # PID y mount namespaces

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

# Ahora los comandos de red ven la red del contenedor
ip addr          # interfaces del contenedor
ip route         # rutas del contenedor
ss -tlnp         # puertos escuchando en el contenedor

# Pero el filesystem sigue siendo el del host
# (no entramos al mount namespace)
```

## unshare — Crear un namespace nuevo

`unshare` crea uno o más namespaces nuevos y ejecuta un comando dentro:

```bash
# Crear un UTS namespace (hostname aislado)
sudo unshare --uts bash
hostname my-isolated-host
hostname
# my-isolated-host

# Salir y verificar que el host no cambió
exit
hostname
# mi-laptop (sin cambios)
```

```bash
# Crear un PID namespace
sudo unshare --pid --fork --mount-proc bash
ps aux
# PID USER  ... COMMAND
#   1 root  ... bash         ← PID 1 es nuestro bash
#   2 root  ... ps aux
# Solo vemos los procesos dentro del namespace

exit
# De vuelta al host, todos los procesos siguen ahí
```

### Flags importantes de unshare

| Flag | Qué hace |
|---|---|
| `--pid` | Nuevo PID namespace |
| `--fork` | Fork antes de ejecutar (necesario con --pid) |
| `--mount-proc` | Montar un /proc nuevo (para que ps funcione) |
| `--mount` / `--propagation private` | Nuevo mount namespace |
| `--uts` | Nuevo UTS namespace |
| `--ipc` | Nuevo IPC namespace |
| `--net` | Nuevo network namespace |
| `--user` | Nuevo user namespace |
| `--map-root-user` | Mapear UID/GID actual a root dentro del user namespace |

```bash
# --fork es necesario con --pid porque:
# unshare crea el namespace pero el proceso que lo creó NO entra
# al PID namespace como PID 1 (es el parent del namespace).
# --fork hace que unshare haga fork() y el hijo sea PID 1.

# --mount-proc es necesario porque:
# /proc muestra los procesos del PID namespace del kernel que lo montó.
# Sin remontarlo, ps aux mostraría TODOS los procesos del host.
```

## Namespaces y Docker

Docker usa **todos** los namespaces (excepto time, que es más reciente):

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

  2. Configura la red (veth pair + bridge)
  3. Monta el rootfs de la imagen
  4. Monta /proc, /sys, /dev
  5. pivot_root al rootfs
  6. Ejecuta el entrypoint (sh en este caso)
```

```bash
# Ver los namespaces de un contenedor vs el host
docker run -d --name test alpine sleep 300
CPID=$(docker inspect --format '{{.State.Pid}}' test)

echo "=== Host (PID 1) ==="
ls -la /proc/1/ns/ | awk '{print $NF}'

echo "=== Container ==="
ls -la /proc/$CPID/ns/ | awk '{print $NF}'

# Los inodes son diferentes → aislados
docker rm -f test
```

## Diferencias entre distribuciones

| Aspecto | Debian/Ubuntu | RHEL/AlmaLinux |
|---|---|---|
| User namespaces | Habilitados por defecto | Habilitados por defecto (RHEL 8+) |
| unshare | Paquete `util-linux` | Paquete `util-linux` |
| nsenter | Paquete `util-linux` | Paquete `util-linux` |
| lsns | Paquete `util-linux` | Paquete `util-linux` |
| Restricciones | `kernel.unprivileged_userns_clone` puede estar 0 | Sin restricción extra |

```bash
# Debian/Ubuntu: puede restringir user namespaces para no-root
sysctl kernel.unprivileged_userns_clone
# 1 = permitido (default en la mayoría)
# 0 = solo root puede crear user namespaces
```

---

## Ejercicios

### Ejercicio 1 — Ver tus namespaces

```bash
# Ver los namespaces de tu shell
ls -la /proc/$$/ns/

# Listar todos los namespaces del sistema
lsns

# ¿Cuántos namespaces de red diferentes hay?
lsns -t net
# Si hay contenedores corriendo, habrá más de uno
```

### Ejercicio 2 — UTS namespace

```bash
# Crear un UTS namespace aislado
sudo unshare --uts bash

# Cambiar el hostname
hostname namespace-test
hostname
# namespace-test

# Salir
exit

# ¿Cambió el hostname del host?
hostname
```

### Ejercicio 3 — PID namespace

```bash
# Crear un PID namespace
sudo unshare --pid --fork --mount-proc bash

# ¿Cuántos procesos ves?
ps aux

# ¿Cuál es tu PID?
echo $$
# Debería ser 1 (eres PID 1 en este namespace)

# Salir
exit
```
