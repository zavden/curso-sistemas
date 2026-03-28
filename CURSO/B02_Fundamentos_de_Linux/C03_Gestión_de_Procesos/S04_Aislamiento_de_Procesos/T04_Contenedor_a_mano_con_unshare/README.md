# T04 — Contenedor a mano con unshare

## El objetivo

En este tópico construimos un "contenedor" usando solo herramientas del
sistema: `unshare`, `chroot`, y `mount`. No Docker, no Podman, no runc.
El resultado es funcionalmente equivalente a `docker run -it alpine sh`.

```
Lo que Docker hace internamente:
  1. Desempaquetar la imagen (rootfs)
  2. Crear namespaces (PID, mount, UTS, IPC, net)
  3. Configurar cgroup (límites de recursos)
  4. pivot_root al rootfs
  5. Montar /proc, /sys, /dev
  6. Ejecutar el entrypoint

Nosotros vamos a hacer lo mismo a mano.
```

## Paso 1 — Preparar el rootfs

Necesitamos un filesystem raíz completo. La forma más sencilla es exportar
una imagen de Alpine desde Docker:

```bash
# Crear directorio para el rootfs
mkdir -p /tmp/container/rootfs

# Exportar Alpine (solo 5MB)
docker export $(docker create alpine) | tar -xf - -C /tmp/container/rootfs

# Verificar que tenemos un rootfs completo
ls /tmp/container/rootfs/
# bin  dev  etc  home  lib  media  mnt  opt  proc  root
# run  sbin srv  sys   tmp  usr    var

# Alternativa sin Docker: descargar el rootfs de Alpine directamente
# curl -o alpine.tar.gz https://dl-cdn.alpinelinux.org/alpine/v3.19/releases/x86_64/alpine-minirootfs-3.19.0-x86_64.tar.gz
# tar -xzf alpine.tar.gz -C /tmp/container/rootfs
```

## Paso 2 — Crear el contenedor

El comando completo que crea todos los namespaces y entra al rootfs:

```bash
sudo unshare \
    --pid \
    --fork \
    --mount \
    --uts \
    --ipc \
    --net \
    --mount-proc=/tmp/container/rootfs/proc \
    chroot /tmp/container/rootfs \
    /bin/sh
```

### Desglose de cada flag

```
--pid                  Nuevo PID namespace → PID 1 propio
--fork                 Fork para que el hijo sea PID 1 en el namespace
--mount                Nuevo mount namespace → mounts aislados
--uts                  Nuevo UTS namespace → hostname propio
--ipc                  Nuevo IPC namespace → IPC aislado
--net                  Nuevo network namespace → red aislada (vacía)
--mount-proc=...       Montar /proc dentro del rootfs (para ps, top)
chroot .../rootfs      Cambiar la raíz al rootfs de Alpine
/bin/sh                El "entrypoint" de nuestro contenedor
```

## Paso 3 — Verificar el aislamiento

Una vez dentro del "contenedor", verificar cada dimensión de aislamiento:

### PID namespace

```bash
# Dentro del contenedor:
ps aux
# PID   USER     TIME  COMMAND
#     1 root      0:00 /bin/sh
#     2 root      0:00 ps aux

# Solo vemos nuestros procesos. PID 1 es nuestro shell.
# Los procesos del host son invisibles.

echo $$
# 1
```

### UTS namespace (hostname)

```bash
# Dentro del contenedor:
hostname
# el-hostname-del-host (heredado al crear)

# Pero podemos cambiarlo sin afectar al host
hostname mi-contenedor
hostname
# mi-contenedor

# En otra terminal del HOST:
hostname
# mi-laptop (sin cambios)
```

### Mount namespace

```bash
# Dentro del contenedor:
mount
# Solo muestra los mounts del contenedor

df -h
# Solo el rootfs del contenedor

# Montar algo no afecta al host
mount -t tmpfs tmpfs /tmp
# Esto es visible solo dentro del contenedor
```

### Network namespace

```bash
# Dentro del contenedor:
ip addr
# 1: lo: <LOOPBACK> mtu 65536
#     link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
#     (DOWN — ni siquiera está levantada)

ip route
# (vacío — no hay rutas)

# No hay conectividad de red
ping 8.8.8.8
# ping: sendto: Network is unreachable

# El network namespace está completamente vacío
# Docker configura veth pairs y un bridge — nosotros no lo hicimos
```

### Filesystem

```bash
# Dentro del contenedor:
cat /etc/os-release
# Alpine Linux v3.19

ls /home/
# (vacío — es el /home de Alpine, no del host)

# No podemos ver el filesystem del host
ls /host-home 2>/dev/null
# No such file or directory
```

## Paso 4 — Comparar con Docker

```bash
# Abrir otra terminal y ejecutar:
docker run --rm -it alpine sh

# Dentro del contenedor Docker:
ps aux         # PID 1 = sh
hostname       # un hash aleatorio
ip addr        # eth0 con IP asignada (Docker configura la red)
cat /etc/os-release  # Alpine

# La experiencia es prácticamente idéntica
# La diferencia: Docker también configuró la red, los cgroups,
# las capabilities, seccomp, y usó pivot_root en lugar de chroot
```

## Lo que falta vs Docker real

Nuestro contenedor manual tiene limitaciones que Docker resuelve:

| Aspecto | Nuestro contenedor | Docker |
|---|---|---|
| PID aislado | Sí | Sí |
| Hostname aislado | Sí | Sí |
| Filesystem aislado | Sí (chroot) | Sí (pivot_root — más seguro) |
| Red | Vacía (sin conectividad) | Configurada (veth + bridge + NAT) |
| Límites de recursos | No | Sí (cgroups) |
| Captura de señales | Básica | tini/--init para PID 1 |
| Seguridad | chroot (escapable) | seccomp + capabilities + pivot_root |
| User namespace | No | Opcional (rootless mode) |
| Imágenes en capas | No (rootfs plano) | Sí (overlay2/overlayfs) |
| Volúmenes | No | Sí (bind mounts, volumes) |

## Agregar red al contenedor (opcional)

Si se quiere agregar conectividad de red, hay que crear un par veth y
configurar NAT. Este es el mismo mecanismo que usa Docker:

```bash
# FUERA del contenedor (en el host), en otra terminal:

# 1. Identificar el PID del proceso dentro del contenedor
CPID=$(pgrep -f 'unshare.*chroot')

# 2. Crear un par veth
sudo ip link add veth-host type veth peer name veth-cont

# 3. Mover un extremo al network namespace del contenedor
sudo ip link set veth-cont netns /proc/$CPID/ns/net

# 4. Configurar el extremo del host
sudo ip addr add 10.0.0.1/24 dev veth-host
sudo ip link set veth-host up

# 5. Configurar el extremo del contenedor (con nsenter)
sudo nsenter -t $CPID --net ip addr add 10.0.0.2/24 dev veth-cont
sudo nsenter -t $CPID --net ip link set veth-cont up
sudo nsenter -t $CPID --net ip link set lo up
sudo nsenter -t $CPID --net ip route add default via 10.0.0.1

# 6. Habilitar forwarding y NAT en el host
sudo sysctl -w net.ipv4.ip_forward=1
sudo iptables -t nat -A POSTROUTING -s 10.0.0.0/24 -j MASQUERADE

# Ahora dentro del contenedor:
# ping 10.0.0.1    → funciona (host)
# ping 8.8.8.8     → funciona (internet via NAT)
```

Esto es exactamente lo que Docker hace al crear un contenedor con la red
bridge por defecto, pero automatizado.

## Limpieza

```bash
# Dentro del contenedor:
exit

# En el host:
sudo rm -rf /tmp/container

# Si configuraste red:
sudo ip link del veth-host 2>/dev/null
sudo iptables -t nat -D POSTROUTING -s 10.0.0.0/24 -j MASQUERADE 2>/dev/null
```

Después de salir del contenedor, en el host no queda ningún rastro de los
namespaces — el kernel los destruye automáticamente cuando el último proceso
dentro de ellos termina.

## El panorama completo

```
Lo que aprendimos en esta sección (S04):

T01 Namespaces   → Qué ve el proceso (aislamiento de visibilidad)
T02 cgroups      → Cuánto puede usar (limitación de recursos)
T03 chroot       → Dónde vive el proceso (filesystem raíz)
T04 Este tópico  → Integración: un contenedor = namespaces + rootfs

Un contenedor NO es una VM.
Un contenedor ES un proceso Linux con namespaces y cgroups.
No hay magia — son features del kernel combinadas.

En B06/C09 implementaremos lo mismo en C usando clone(),
pivot_root() y la API directa de cgroups.
```

---

## Ejercicios

### Ejercicio 1 — Contenedor mínimo

```bash
# Preparar rootfs
mkdir -p /tmp/mini-container/rootfs
docker export $(docker create alpine) | tar -xf - -C /tmp/mini-container/rootfs

# Crear el contenedor
sudo unshare --pid --fork --mount --uts --ipc --net \
    --mount-proc=/tmp/mini-container/rootfs/proc \
    chroot /tmp/mini-container/rootfs /bin/sh

# Dentro: verificar
ps aux          # ¿cuántos procesos?
echo $$         # ¿cuál es tu PID?
hostname test   # ¿puedes cambiar el hostname?
ip addr         # ¿tienes red?

exit
sudo rm -rf /tmp/mini-container
```

### Ejercicio 2 — Comparar namespaces host vs contenedor

```bash
# Preparar
mkdir -p /tmp/ns-compare/rootfs
docker export $(docker create alpine) | tar -xf - -C /tmp/ns-compare/rootfs

# Anotar los namespaces del host
echo "=== Host ==="
ls -la /proc/$$/ns/ | awk '{print $NF}'

# Crear contenedor en background para inspeccionar
sudo unshare --pid --fork --mount --uts --ipc --net \
    --mount-proc=/tmp/ns-compare/rootfs/proc \
    chroot /tmp/ns-compare/rootfs sleep 60 &
sleep 1

CPID=$(pgrep -f 'sleep 60' | tail -1)
echo "=== Contenedor (PID en host: $CPID) ==="
ls -la /proc/$CPID/ns/ | awk '{print $NF}'

# ¿Son iguales o diferentes los namespace IDs?

kill $CPID 2>/dev/null
sudo rm -rf /tmp/ns-compare
```

### Ejercicio 3 — Docker vs manual

```bash
# Ejecutar ambos y comparar
echo "=== Docker ==="
docker run --rm alpine sh -c 'ps aux; echo "---"; ip addr; echo "---"; hostname'

echo ""
echo "=== Manual ==="
mkdir -p /tmp/compare/rootfs
docker export $(docker create alpine) | tar -xf - -C /tmp/compare/rootfs

sudo unshare --pid --fork --mount --uts --net \
    --mount-proc=/tmp/compare/rootfs/proc \
    chroot /tmp/compare/rootfs \
    sh -c 'ps aux; echo "---"; ip addr; echo "---"; hostname'

sudo rm -rf /tmp/compare

# ¿Qué diferencias observas?
# (Docker configura la red, nuestro contenedor no tiene red)
```
