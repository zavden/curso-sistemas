# T04 — Contenedor a mano con unshare

## Tabla de Contenidos

1. [El objetivo](#el-objetivo)
2. [Paso 1 — Preparar el rootfs](#paso-1--preparar-el-rootfs)
3. [Paso 2 — Crear el contenedor](#paso-2--crear-el-contenedor)
4. [Paso 3 — Verificar el aislamiento](#paso-3--verificar-el-aislamiento)
5. [Paso 4 — Comparar con Docker](#paso-4--comparar-con-docker)
6. [Lo que falta vs Docker real](#lo-que-falta-vs-docker-real)
7. [Agregar red al contenedor](#agregar-red-al-contenedor-opcional)
8. [Limpieza](#limpieza)
9. [El panorama completo de S04](#el-panorama-completo-de-s04)
10. [Ejercicios](#ejercicios)

---

## El objetivo

En este tópico construimos un **contenedor funcional desde cero** usando únicamente herramientas del sistema: `unshare`, `chroot`, y `mount`. Sin Docker, sin Podman, sin runc. El resultado es funcionalmente equivalente a ejecutar `docker run -it alpine sh`.

```
Lo que Docker hace internamente:
  1. Desempaquetar la imagen (rootfs)
  2. Crear namespaces (PID, mount, UTS, IPC, net, user)
  3. Configurar cgroup (límites de recursos)
  4. pivot_root al rootfs
  5. Montar /proc, /sys, /dev
  6. Ejecutar el entrypoint

Nosotros vamos a hacer exactamente lo mismo.
```

**Requisitos:**
- `unshare` (parte de util-linux)
- `chroot`
- `mount`
- Acceso a un rootfs de Alpine (via Docker o descarga directa)
- Privilegios de root

---

## Paso 1 — Preparar el rootfs

Un rootfs (root filesystem) es un directorio que contiene todo lo necesario para que un sistema operativo funcione: binaries, librerías, archivos de configuración, etc. Para un contenedor mínimo de Alpine, ocupa ~5MB.

### Método recomendado: Docker export

```bash
# Crear directorio para el rootfs
mkdir -p /tmp/container/rootfs

# Exportar Alpine: docker create obtiene la imagen, docker export la convierte a tar
docker export $(docker create alpine) | tar -xf - -C /tmp/container/rootfs

# Verificar que tenemos un rootfs completo
ls /tmp/container/rootfs/
# bin  dev  etc  home  lib  media  mnt  opt  proc  root
# run  sbin srv  sys  tmp  usr  var

# Verificar que tiene un shell
ls /tmp/container/rootfs/bin/
# sh  busybox ...
```

### Método alternativo: Descarga directa (sin Docker)

```bash
mkdir -p /tmp/container/rootfs

# Descargar el rootfs de Alpine directamente
curl -o /tmp/alpine.tar.gz \
    https://dl-cdn.alpinelinux.org/alpine/v3.19/releases/x86_64/alpine-minirootfs-3.19.0-x86_64.tar.gz

# Extraer
tar -xzf /tmp/alpine.tar.gz -C /tmp/container/rootfs

# Limpiar
rm /tmp/alpine.tar.gz

# Verificar
ls /tmp/container/rootfs/
# bin  dev  etc  home  lib  media  mnt  opt  proc  root ...
```

### Estructura de un rootfs mínimo de Alpine

```
/tmp/container/rootfs/
├── bin/           → shell (busybox), comandos básicos
├── dev/           → dispositivos (vacío inicialmente)
├── etc/           → configuración del sistema
│   ├── os-release
│   ├── passwd
│   ├── group
│   └── ...
├── home/          → directorios de usuarios
├── lib/           → librerías compartidas
├── media/         → medios removibles
├── mnt/           → puntos de montaje
├── opt/           → aplicaciones opcionales
├── proc/          → sistema de archivos proc (vacío)
├── root/          → directorio del usuario root
├── run/           → archivos de estado runtime
├── sbin/          → binaries de administración
├── srv/           → datos de servicios
├── sys/           → sistema de archivos sys (vacío)
├── tmp/           → archivos temporales
├── usr/           → recursos de usuario (bin, lib, share)
└── var/           → datos variables (log, spool, etc.)
```

---

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

| Flag | Namespace | Qué aísla | Efecto |
|------|-----------|-----------|--------|
| `--pid` | PID namespace | Identificadores de proceso | Nuestro shell será PID 1 |
| `--fork` | — | Creación de procesos | Hace fork para que el hijo sea PID 1 |
| `--mount` | Mount namespace | Puntos de montaje | Los mounts son privados |
| `--uts` | UTS namespace | hostname, domainname | Podemos tener hostname propio |
| `--ipc` | IPC namespace | Memorias compartidas, colas | Aislamiento de IPC |
| `--net` | Network namespace | Interfaces, puertos, rutas | Red aislada |
| `--mount-proc` | — | Flag especial | Monta /proc en el rootfs |
| `chroot .../rootfs` | — | Directorio raíz | Usa rootfs como "/ " |
| `/bin/sh` | — | Entry point | Shell interactivo |

### Qué sucede al ejecutar el comando

```
Secuencia de eventos:

1. unshare crea nuevos namespaces (mount, UTS, IPC, net, PID)
2. Se hace fork: el proceso hijo será PID 1 del nuevo PID namespace
3. El proceso hijo ejecuta chroot al rootfs de Alpine
4. Se monta /proc (para que ps funcione)
5. Se ejecuta /bin/sh
6. El usuario obtiene un shell interactivo
```

### Versión mínima vs completa

```bash
# Versión MÍNIMA (solo PID y filesystem):
sudo unshare --pid --fork --mount-proc=/tmp/container/rootfs/proc \
    chroot /tmp/container/rootfs /bin/sh

# Versión COMPLETA (todos los namespaces):
sudo unshare --pid --fork --mount --uts --ipc --net \
    --mount-proc=/tmp/container/rootfs/proc \
    chroot /tmp/container/rootfs /bin/sh

# Ambas funcionan, pero la segunda proporciona más aislamiento
```

---

## Paso 3 — Verificar el aislamiento

Una vez dentro del "contenedor", verificar cada dimensión de aislamiento:

### PID namespace

```bash
# Dentro del contenedor:
ps aux
# PID   USER   TIME  COMMAND
#     1 root    0:00 /bin/sh     ← nuestro shell es PID 1
#     2 root    0:00 ps aux      ← ps es nuestro único proceso

# Verificar PID propio
echo $$
# 1

# Intentar ver procesos del host:
cat /proc/1/cmdline
# (vacío o inaccesible)

# Solo existen 2 procesos en este namespace
ps aux | wc -l
# 2 (sh + ps)
```

**Comparación:**

| En el contenedor | En el host |
|------------------|------------|
| PID 1 = /bin/sh | PID 1 = /sbin/init o systemd |
| Solo 2-3 procesos visibles | Hundreds de procesos |
| No hay procesos del host | El contenedor no es visible |

### UTS namespace (hostname)

```bash
# Dentro del contenedor:

# Ver hostname actual (heredado del host al crear)
hostname
# el-hostname-del-host

# Cambiar hostname (solo afecta a este namespace)
hostname mi-contenedor
hostname
# mi-contenedor

# Verificar que el host no cambió (desde OTRA terminal del host):
# $ hostname
# mi-laptop                    ← sin cambios
```

### Mount namespace

```bash
# Dentro del contenedor:

# Ver mounts (solo los del contenedor)
mount | head -20
# /dev/sda1 on / type ext4 (rw,relatime)
# /dev/sda1 on /tmp type ext4 (rw,relatime)
# tmpfs on /tmp type tmpfs (rw,nosuid,nodev)

# df -h muestra solo el disco del contenedor
df -h
# Filesystem      Size  Used Avail Use% Mounted on
# /dev/sda1        ...   ...   ...  ... /
# tmpfs            ...   ...   ...  ... /tmp

# Montar algo nuevo (solo existe dentro del contenedor)
mount -t tmpfs tmpfs /misdatos
ls /misdatos
# (vacío)

# Verificar que el host no ve este mount
# (Desde el host: ls /misdatos → No such file or directory)
```

### Network namespace

```bash
# Dentro del contenedor:

# Ver interfaces de red
ip addr
# 1: lo: <LOOPBACK> mtu 65536
#     link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
#     state UNKNOWN
#     (DOWN — ni siquiera está levantada)

# El namespace está completamente vacío
ip route
# (vacío — no hay rutas)

# Intentar conectividad
ping 8.8.8.8
# ping: sendto: Network is unreachable

# No hay eth0, no hay IP, no hay nada
```

**Comparación de red:**

| Aspecto | Nuestro contenedor | Docker |
|---------|---------------------|--------|
| Interfaces | Solo lo (DOWN) | eth0 con IP |
| Rutas | Ninguna | Gateway configurado |
| DNS | No configurado | /etc/resolv.conf |
| Conectividad | Ninguna | NAT hacia host |

### Filesystem

```bash
# Dentro del contenedor:

# La distribución es Alpine
cat /etc/os-release
# NAME="Alpine Linux"
# VERSION_ID=v3.19
# ID=alpine

# El /home está vacío (es el de Alpine, no del host)
ls /home/
# (vacío)

# Intentamos ver archivos del host
ls /host
# No such file or directory

ls /proc/1         # Intentar ver info del PID 1 del host
# (inaccesible o vacío)

# Montar algo en /tmp
mount -t tmpfs tmpfs /tmp
touch /tmp/test.txt
ls /tmp/
# test.txt
```

### Comparación rápida: dentro vs fuera

```bash
# Dentro del contenedor:
echo "=== CONTENEDOR ==="
echo "PID propio: $$"
hostname
ip addr | grep 'inet '
cat /proc/1/cgroup | head -3
ls /proc/1/cmdline 2>&1

# Fuera del contenedor (en el host), ejecutar:
# echo "=== HOST ==="
# echo "PID propio: $$"
# hostname
# ip addr | grep 'inet '
# cat /proc/1/cgroup | head -3
```

---

## Paso 4 — Comparar con Docker

Ejecutar ambos y comparar resultados:

```bash
# Abrir dos terminales

# Terminal 1: Contenedor manual
sudo unshare --pid --fork --mount --uts --ipc --net \
    --mount-proc=/tmp/container/rootfs/proc \
    chroot /tmp/container/rootfs /bin/sh

# Terminal 2: Contenedor Docker
docker run --rm -it alpine sh
```

### Comparación lado a lado

| Característica | Contenedor manual | Docker |
|----------------|-------------------|--------|
| PID 1 = sh | Sí | Sí |
| Hostname propio | Sí (hay que configurarlo) | Sí (hash aleatorio) |
| Red | Vacía (no hay veth) | eth0 con IP (bridge) |
| /proc/ps | Funciona | Funciona |
| Isolación de procesos | Sí (PID ns) | Sí (PID ns) |
| Aislamiento de mounts | Sí (mount ns) | Sí (mount ns) |
| Aislamiento IPC | Sí (IPC ns) | Sí (IPC ns) |

---

## Lo que falta vs Docker real

Nuestro contenedor manual, aunque funcional, carece de muchas características de Docker:

| Aspecto | Nuestro contenedor | Docker real |
|---------|---------------------|-------------|
| PID aislado | Sí | Sí |
| Hostname aislado | Sí | Sí |
| Filesystem aislado | Sí (chroot) | Sí (pivot_root — más seguro) |
| Red | Vacía | Configurada (veth pair + bridge + NAT) |
| Límites de memoria | No | Sí (cgroups) |
| Límites de CPU | No | Sí (cgroups) |
| PIDs permitidos | Ilimitados | Limitados (cgroups) |
| Captura de señales | Básica | tini/proper PID 1 |
| Acceso a archivos | chroot (escapable) | pivot_root (no escapable) |
| User namespace | No | Opcional (rootless) |
| Imágenes en capas | No (rootfs plano) | Sí (overlay2/overlayfs) |
| Volúmenes | No | Sí (bind mounts, volumes) |
| Logs | No (stdout se pierde) | Sí (logging driver) |
| Reinicio automático | No | Sí (restart policy) |
| Redes predefinidas | No | bridge, host, none, custom |

**La diferencia más importante:** Docker usa `pivot_root` en lugar de `chroot`, lo que hace que el escape sea imposible (el viejo rootfs se desmonta completamente). También configura `cgroups` para limitar recursos y capabilities para restringir privilegios.

---

## Agregar red al contenedor (opcional)

Para dar conectividad de red a nuestro contenedor manual, necesitamos crear un par veth y configurar NAT. Esto es exactamente lo que Docker hace automáticamente.

### Topología de red que queremos

```
Contenedor                      Host
────────────                    ─────
   lo (127.0.0.1)              lo
   veth-cont (10.0.0.2) ←────→ veth-host (10.0.0.1)
                               ↓
                           eth0 → Internet
                           iptables NAT
```

### Paso 1: Identificar el PID del contenedor

```bash
# FUERA del contenedor, crear el contenedor en background
sudo unshare --pid --fork --mount --uts --ipc --net \
    --mount-proc=/tmp/container/rootfs/proc \
    chroot /tmp/container/rootfs sleep 300 &
sleep 1

# Obtener el PID del proceso sleep (que es PID 1 dentro del namespace)
CPID=$(pgrep -f 'sleep 300' | tail -1)
echo "PID del contenedor en el host: $CPID"
```

### Paso 2: Crear el par veth

```bash
# Crear un par de interfaces veth (virtual ethernet)
# veth-host: el lado del host
# veth-cont: el lado del contenedor
sudo ip link add veth-host type veth peer name veth-cont

# Configurar la IP del host
sudo ip addr add 10.0.0.1/24 dev veth-host
sudo ip link set veth-host up
```

### Paso 3: Mover veth-cont al network namespace del contenedor

```bash
# Mover el extremo veth-cont al network namespace del contenedor
sudo ip link set veth-cont netns /proc/$CPID/ns/net

# Verificar: ahora el host no tiene veth-cont
ip link | grep veth-cont
# (no debería aparecer)
```

### Paso 4: Configurar la red dentro del contenedor

```bash
# Entrar al network namespace del contenedor y configurar
sudo nsenter -t $CPID --net ip addr add 10.0.0.2/24 dev veth-cont
sudo nsenter -t $CPID --net ip link set veth-cont up
sudo nsenter -t $CPID --net ip link set lo up

# Agregar ruta por defecto
sudo nsenter -t $CPID --net ip route add default via 10.0.0.1
```

### Paso 5: Configurar NAT en el host

```bash
# Habilitar IP forwarding
sudo sysctl -w net.ipv4.ip_forward=1

# Configurar NAT (masquerading)
sudo iptables -t nat -A POSTROUTING -s 10.0.0.0/24 -j MASQUERADE
```

### Paso 6: Verificar conectividad

```bash
# Dentro del contenedor (usando nsenter):
sudo nsenter -t $CPID --net /bin/sh -c '
    ping -c 2 10.0.0.1     # ping al host
    ping -c 2 8.8.8.8       # ping a internet
    wget -q -O- http://example.com  # descargar página
'
```

### Limpieza de red

```bash
# Eliminar el par veth
sudo ip link del veth-host 2>/dev/null

# Eliminar regla NAT
sudo iptables -t nat -D POSTROUTING -s 10.0.0.0/24 -j MASQUERADE 2>/dev/null

# Matar el contenedor
sudo kill $CPID 2>/dev/null
```

---

## Limpieza

### Limpieza básica

```bash
# Salir del contenedor
exit

# En el host, eliminar el rootfs
sudo rm -rf /tmp/container
```

### Limpieza completa con red

```bash
# Matar el contenedor si está corriendo
sudo kill $(pgrep -f 'unshare.*sleep 300') 2>/dev/null

# Eliminar la red si se creó
sudo ip link del veth-host 2>/dev/null
sudo iptables -t nat -D POSTROUTING -s 10.0.0.0/24 -j MASQUERADE 2>/dev/null

# Eliminar el rootfs
sudo rm -rf /tmp/container
```

### Nota sobre namespaces

```
Después de salir del contenedor, en el host no queda ningún rastro:
  → Los namespaces se destruyen automáticamente cuando el último proceso
    dentro de ellos termina
  → Los mounts desaparecen
  → Los recursos de red se limpian

Esto es diferente de Docker, que también limpia después de docker rm,
pero los namespaces del kernel se destruyen igual cuando el último
proceso del namespace termina.
```

---

## El panorama completo de S04

```
Lo que aprendimos en esta sección (S04):

┌─────────────────────────────────────────────────────────────┐
│ T01 Namespaces     → QUÉ ve el proceso (aislamiento de      │
│                      visibilidad por categoría de recurso)   │
│                                                              │
│ T02 cgroups v2     → CUÁNTO puede usar (límites de CPU,     │
│                      memoria, PIDs)                          │
│                                                              │
│ T03 chroot/pivot   → DÓNDE vive el proceso (filesystem      │
│                      raíz y su manipulación)                │
│                                                              │
│ T04 Este tópico    → INTEGRACIÓN: un contenedor =           │
│                      namespaces + rootfs + cgroups           │
└─────────────────────────────────────────────────────────────┘

Concepto fundamental:

  Un contenedor NO es una VM
  Un contenedor ES un proceso Linux con namespaces + cgroups
  No hay magia — son features del kernel combinadas

  + namespaces   → aislamiento de "qué ve" (UTS, PID, NET, IPC, MOUNT, USER)
  + cgroups      → aislamiento de "cuánto usa" (CPU, memoria, PIDs)
  + pivot_root   → filesystem propio (no escapable)
  + capabilities → privilegios reducidos
  + seccomp      → syscalls restringidos

En B06/C09 implementaremos lo mismo en C usando:
  → clone()         para crear procesos con namespaces
  → pivot_root()    para cambiar la raíz
  → la API de cgroups v2 para límites de recursos
```

---

## Ejercicios

### Ejercicio 1 — Contenedor mínimo

Crear el contenedor más simple posible y verificar el aislamiento básico:

```bash
# Preparar rootfs
mkdir -p /tmp/mini-container/rootfs
docker export $(docker create alpine) | tar -xf - -C /tmp/mini-container/rootfs

# Crear el contenedor
sudo unshare --pid --fork --mount-proc=/tmp/mini-container/rootfs/proc \
    chroot /tmp/mini-container/rootfs /bin/sh

# Dentro del contenedor, responder:
ps aux                    # ¿cuántos procesos ves?
echo $$                   # ¿cuál es tu PID?
cat /etc/os-release       # ¿qué distro?
ls /                      # ¿qué archivos ves?
mount | wc -l             # ¿cuántos mounts?

exit
sudo rm -rf /tmp/mini-container
```

### Ejercicio 2 — Comparar namespaces del host vs contenedor

Comparar los namespaces antes y después de crear el contenedor:

```bash
# Preparar
mkdir -p /tmp/ns-compare/rootfs
docker export $(docker create alpine) | tar -xf - -C /tmp/ns-compare/rootfs

# Anotar los namespaces del host
echo "=== Namespaces del HOST ==="
ls -la /proc/$$/ns/ | awk '{print $NF, $NF}'
# Verás: mnt, pid, user, uts, ipc, net

# Crear contenedor en background para inspeccionar
sudo unshare --pid --fork --mount --uts --ipc --net \
    --mount-proc=/tmp/ns-compare/rootfs/proc \
    chroot /tmp/ns-compare/rootfs sleep 60 &
sleep 1

CPID=$(pgrep -f 'sleep 60' | tail -1)
echo ""
echo "=== Namespaces del CONTENEDOR (PID en host: $CPID) ==="
ls -la /proc/$CPID/ns/ | awk '{print $NF}'

echo ""
echo "=== Comparación ==="
echo "Host PID namespace:"
readlink /proc/$$/ns/pid
echo "Contenedor PID namespace:"
readlink /proc/$CPID/ns/pid
echo ""
echo "¿Son iguales o diferentes?"

# Comparar TODOS los namespaces
for ns in mnt pid user uts ipc net; do
    echo -n "$ns: host=$(readlink /proc/$$/ns/$ns) "
    echo "container=$(readlink /proc/$CPID/ns/$ns)"
done

kill $CPID 2>/dev/null
sudo rm -rf /tmp/ns-compare
```

### Ejercicio 3 — Docker vs manual comparados

Ejecutar ambos y observar diferencias:

```bash
echo "=== Docker (esperar unos segundos) ==="
docker run --rm alpine sh -c '
    echo "Hostname: $(hostname)"
    echo "PID propio: $$"
    echo "---"
    echo "Procesos:"
    ps aux
    echo "---"
    echo "Red:"
    ip addr
    echo "---"
    echo "Distro:"
    cat /etc/os-release | grep PRETTY
'

echo ""
echo "=== Manual (con todos los namespaces) ==="
mkdir -p /tmp/compare/rootfs
docker export $(docker create alpine) | tar -xf - -C /tmp/compare/rootfs

sudo unshare --pid --fork --mount --uts --net --ipc \
    --mount-proc=/tmp/compare/rootfs/proc \
    chroot /tmp/compare/rootfs sh -c '
    hostname test-container
    echo "Hostname: $(hostname)"
    echo "PID propio: $$"
    echo "---"
    echo "Procesos:"
    ps aux
    echo "---"
    echo "Red:"
    ip addr
    echo "---"
    echo "Distro:"
    cat /etc/os-release | grep PRETTY
'

sudo rm -rf /tmp/compare

# Preguntas de análisis:
# 1. ¿Qué diferencia hay en el hostname inicial?
# 2. ¿Por qué Docker tiene eth0 pero el manual no?
# 3. ¿Cuántos procesos ves en cada caso?
```

### Ejercicio 4 — Crear un "servicio" dentro del contenedor

Simular un servicio que sigue corriendo:

```bash
# Preparar
mkdir -p /tmp/service-container/rootfs
docker export $(docker create alpine) | tar -xf - -C /tmp/service-container/rootfs

# Crear un script de "servicio"
cat > /tmp/service-container/rootfs/start.sh << 'EOF'
#!/bin/sh
echo "Servicio iniciado con PID $$" >> /tmp/service.log
while true; do
    echo "$(date): Servicio vivo" >> /tmp/service.log
    sleep 5
done
EOF
chmod +x /tmp/service-container/rootfs/start.sh

# Crear el contenedor con el servicio
sudo unshare --pid --fork --mount --uts --ipc --net \
    --mount-proc=/tmp/service-container/rootfs/proc \
    chroot /tmp/service-container/rootfs /bin/sh -c '
    hostname mi-servicio
    echo "Contenedor iniciado"
    /start.sh &
    echo "PID del shell: $$"
    echo "PID del servicio: $!"
    sleep 2
    echo "--- Procesos ---"
    ps aux
    echo "--- Log ---"
    cat /tmp/service.log
    exit
'

# Verificar logs (si el contenedor aún está corriendo)
# cat /tmp/service-container/rootfs/tmp/service.log

sudo rm -rf /tmp/service-container
```

### Ejercicio 5 — Investigar los namespaces desde /proc

Explorar los archivos de /proc que revelan información sobre namespaces:

```bash
# Desde el host, ver los namespaces de un proceso
ls -la /proc/$$/ns/
# Muestra todos los namespaces de tu shell actual

# Ver qué procesos comparten namespaces
# (todos los procesos en el mismo network namespace)
sudo ls -la /proc/*/ns/net 2>/dev/null | awk '{print $NF}' | sort | uniq -c

# Ver los cgroups de un proceso
cat /proc/$$/cgroup

# Ver los limits de un proceso (los que aplica cgroups)
cat /proc/$$/limits

# Crear un contenedor y comparar
mkdir -p /tmp/ns-proc/rootfs
docker export $(docker create alpine) | tar -xf - -C /tmp/ns-proc/rootfs

sudo unshare --pid --fork --mount --uts --ipc --net \
    --mount-proc=/tmp/ns-proc/rootfs/proc \
    chroot /tmp/ns-proc/rootfs sleep 60 &
CPID=$(pgrep -f 'sleep 60' | tail -1)

echo "=== Host process $$ ==="
echo "cgroups:"
cat /proc/$$/cgroup | head -5
echo "limits:"
cat /proc/$$/limits | grep -E 'Max processes|Max files'

echo ""
echo "=== Container process $CPID ==="
echo "cgroups:"
cat /proc/$CPID/cgroup | head -5
echo "limits:"
cat /proc/$CPID/limits | grep -E 'Max processes|Max files'

kill $CPID 2>/dev/null
sudo rm -rf /tmp/ns-proc
```

### Ejercicio 6 — Explorar el panorama completo (preparación para B06)

Investigar cómo se implementaría esto en C:

```bash
# Investigar las syscalls involucradas
echo "=== Syscalls involucradas en contenedores ==="

# Buscar la syscall pivot_root
grep -r pivot_root /usr/include/ 2>/dev/null | head -5
# #include <sys/pivot_root.h>

# Buscar clone
man clone 2>/dev/null | head -20 || echo "man clone no disponible"

# Ver flags de clone relacionados con namespaces
echo ""
echo "Flags de clone para namespaces:"
cat << 'FLAGS'
CLONE_NEWNS   - Mount namespace
CLONE_NEWUTS  - UTS namespace (hostname)
CLONE_NEWIPC  - IPC namespace
CLONE_NEWPID  - PID namespace
CLONE_NEWNET  - Network namespace
CLONE_NEWUSER - User namespace
CLONE_NEWCGROUP - Cgroup namespace
FLAGS

# Ver el mapa de syscalls
# cat /proc/sys/kernel/ns_last_pid  # última PID asignada

echo ""
echo "=== Próximo paso: B06/C09 ==="
echo "En B06/C09 implementaremos contenedores en C usando:"
echo "  clone()       - con CLONE_NEW* flags"
echo "  pivot_root() - para cambiar la raíz"
echo "  setns()       - para unirse a namespaces existentes"
echo "  la API de cgroups v2 - para límites de recursos"
```
