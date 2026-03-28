# T04 — Contenedor a mano con unshare

## Errata y notas sobre el material base

1. **README.max.md: `cat /proc/1/cmdline` → "vacío o inaccesible"** —
   Falso. Dentro del contenedor con PID namespace, PID 1 **es** nuestro
   shell. `cat /proc/1/cmdline` muestra `/bin/sh` (o el entrypoint).
   Sería inaccesible solo si no montamos `/proc`.

2. **README.max.md: "El contenedor no es visible" (desde el host)** —
   Incorrecto. Desde el host, los procesos del contenedor SÍ son
   visibles (con sus PIDs del host). Lo que es cierto: desde DENTRO
   del contenedor, los procesos del host son invisibles.

3. **README.md/README.max.md: `ip link set veth-cont netns /proc/$CPID/ns/net`** —
   Sintaxis incorrecta. `ip link set netns` acepta un **PID** o un
   **nombre de netns**, no una ruta de archivo. Correcto:
   `ip link set veth-cont netns $CPID`.

4. **README.md: `pgrep -f 'unshare.*chroot'`** — Esto captura el
   proceso `unshare` (el padre), no el hijo dentro del namespace. Mejor
   usar `$!` (PID del último proceso en background) o
   `pgrep -f 'sleep 300'`.

5. **README.max.md: `mount | head -20` muestra solo mounts del contenedor** —
   Engañoso. Un mount namespace creado con `unshare --mount` empieza
   como una **copia** de los mounts del padre. Todos los mounts del
   host son inicialmente visibles. Solo los mounts NUEVOS son privados.
   El aislamiento real del filesystem viene de `chroot`/`pivot_root`,
   no del mount namespace solo.

6. **README.max.md ejercicio 2: `awk '{print $NF, $NF}'`** — Typo:
   imprime el último campo dos veces.

7. **Ambos READMEs dicen "funcionalmente equivalente a `docker run`"** —
   Exageración. Le faltan: pivot_root (usa chroot escapable), cgroups,
   capabilities reducidas, seccomp, red configurada, overlay2. Es una
   **demostración educativa**, no un equivalente funcional.

---

## El objetivo

Construir un "contenedor" usando solo herramientas del sistema:
`unshare`, `chroot`, y `mount`. Sin Docker, sin Podman, sin runc.

```
Lo que Docker hace internamente:
  1. Desempaquetar la imagen (rootfs)
  2. clone() con namespaces (PID, mount, UTS, IPC, net, user, cgroup)
  3. Configurar cgroup (límites de recursos)
  4. pivot_root al rootfs
  5. Montar /proc, /sys, /dev
  6. Reducir capabilities + activar seccomp
  7. Ejecutar el entrypoint

Nosotros hacemos una versión simplificada con unshare + chroot.
```

---

## Paso 1 — Preparar el rootfs

Un rootfs (root filesystem) es un directorio con todo lo necesario
para que un sistema funcione: binarios, librerías, configuración.

### Método 1: Exportar imagen Docker (~5 MB)

```bash
mkdir -p /tmp/container/rootfs
docker export $(docker create alpine) | tar -xf - -C /tmp/container/rootfs

ls /tmp/container/rootfs/
# bin  dev  etc  home  lib  media  mnt  opt  proc  root
# run  sbin srv  sys  tmp  usr  var
```

### Método 2: Descarga directa (sin Docker)

```bash
mkdir -p /tmp/container/rootfs
curl -o /tmp/alpine.tar.gz \
    https://dl-cdn.alpinelinux.org/alpine/v3.19/releases/x86_64/alpine-minirootfs-3.19.0-x86_64.tar.gz
tar -xzf /tmp/alpine.tar.gz -C /tmp/container/rootfs
rm /tmp/alpine.tar.gz
```

### Estructura del rootfs

```
/tmp/container/rootfs/
├── bin/     → shell (busybox), comandos básicos
├── dev/     → dispositivos (vacío, se puebla al montar)
├── etc/     → configuración (os-release, passwd, group)
├── lib/     → librerías compartidas (musl libc en Alpine)
├── proc/    → vacío (se monta con --mount-proc)
├── sys/     → vacío (se monta si se necesita)
├── tmp/     → archivos temporales
├── usr/     → binarios y librerías adicionales
└── var/     → datos variables (logs, spool)
```

---

## Paso 2 — Crear el contenedor

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

| Flag | Qué hace | Por qué |
|------|----------|---------|
| `--pid` | Nuevo PID namespace | El shell será PID 1, no ve procesos del host |
| `--fork` | Fork antes de ejecutar | El **hijo** entra como PID 1 (sin fork, el padre queda fuera del namespace) |
| `--mount` | Nuevo mount namespace | Montar/desmontar no afecta al host |
| `--uts` | Nuevo UTS namespace | hostname propio (cambiarlo no afecta al host) |
| `--ipc` | Nuevo IPC namespace | Semáforos y shared memory aislados |
| `--net` | Nuevo network namespace | Red aislada (vacía: solo loopback DOWN) |
| `--mount-proc=...` | Montar /proc nuevo | Para que `ps` muestre solo procesos del namespace |
| `chroot .../rootfs` | Cambiar raíz | Filesystem de Alpine como `/` |
| `/bin/sh` | Entrypoint | Shell interactivo |

### Por qué `--fork` es obligatorio con `--pid`

```
Sin --fork:
  unshare crea el nuevo PID namespace
  PERO el proceso unshare (y el comando que ejecuta) quedan en el
  namespace PADRE — el nuevo namespace no tiene PID 1
  → ps muestra los procesos del host
  → el namespace eventualmente se destruye (sin PID 1)

Con --fork:
  unshare crea el namespace Y hace fork()
  El HIJO entra al namespace como PID 1
  → ps muestra solo 2 procesos (sh + ps)
  → todo funciona como un contenedor

Docker no tiene este problema: clone() con CLONE_NEWPID
crea el hijo directamente como PID 1 del nuevo namespace.
```

### Versión mínima vs completa

```bash
# MÍNIMA (solo PID + filesystem):
sudo unshare --pid --fork --mount-proc=/tmp/container/rootfs/proc \
    chroot /tmp/container/rootfs /bin/sh
# Ve solo sus procesos, pero comparte hostname, red, IPC del host

# COMPLETA (todos los namespaces):
sudo unshare --pid --fork --mount --uts --ipc --net \
    --mount-proc=/tmp/container/rootfs/proc \
    chroot /tmp/container/rootfs /bin/sh
# Más aislado, pero sin red
```

---

## Paso 3 — Verificar el aislamiento

### PID namespace

```bash
# Dentro del contenedor:
ps aux
# PID   USER   COMMAND
#   1   root   /bin/sh         ← nuestro shell es PID 1
#   2   root   ps aux          ← el único otro proceso

echo $$
# 1

# ¿Hay procesos del host?
ps aux | wc -l
# 3 (header + sh + ps) — no hay cientos como en el host

# PID 1 dentro es NUESTRO shell, no systemd
cat /proc/1/cmdline | tr '\0' ' '
# /bin/sh
```

### UTS namespace (hostname)

```bash
hostname
# <hostname-del-host>  (heredado al crear el namespace)

# Cambiarlo — solo afecta a este namespace
hostname mi-contenedor
hostname
# mi-contenedor

# En otra terminal del HOST:
# $ hostname
# mi-laptop  ← sin cambios
```

### Mount namespace

```bash
mount
# Muestra los mounts heredados del host + /proc nuevo

# Montar algo — solo existe dentro del contenedor
mount -t tmpfs tmpfs /tmp
touch /tmp/solo-aqui
# Desde el host: ls /tmp/solo-aqui → No such file

# Nota: un mount namespace empieza como COPIA del padre.
# Los mounts del host son visibles inicialmente.
# Solo los mounts NUEVOS (después del unshare) son privados.
# chroot/pivot_root es lo que realmente cambia el filesystem visible.
```

### Network namespace

```bash
ip addr
# 1: lo: <LOOPBACK> mtu 65536
#     link/loopback 00:00:00:00:00:00
#     (DOWN — ni siquiera está levantada)

ip route
# (vacío — no hay rutas)

ping 8.8.8.8
# Network is unreachable

# El namespace empieza completamente vacío — solo loopback (DOWN)
# Docker configura veth pairs + bridge + NAT automáticamente
# Nosotros no lo hicimos
```

### Filesystem (chroot)

```bash
cat /etc/os-release
# Alpine Linux  (no la distro del host)

ls /home/
# (vacío — es el /home de Alpine)

ls /host 2>/dev/null
# No such file or directory — no hay acceso al host
```

---

## Paso 4 — Comparar con Docker

```bash
# En otra terminal:
docker run --rm -it alpine sh

# Dentro del Docker:
ps aux       # PID 1 = sh           ← igual
hostname     # hash aleatorio       ← Docker lo configura
ip addr      # eth0 con IP          ← Docker configura la red
cat /etc/os-release  # Alpine       ← igual
```

### ¿Qué tiene Docker que no tiene nuestro contenedor?

| Aspecto | Nuestro contenedor | Docker |
|---|---|---|
| PID aislado | Sí | Sí |
| Hostname aislado | Sí | Sí (autoconfigurado) |
| Filesystem | chroot (escapable) | pivot_root (seguro) |
| Red | Vacía | Configurada (veth + bridge + NAT) |
| Límites de recursos | No | Sí (cgroups) |
| Seguridad | Root real | Capabilities reducidas + seccomp |
| User namespace | No | Opcional (rootless) |
| Imágenes en capas | No (tar plano) | Sí (overlay2) |
| Volúmenes | No | Bind mounts + named volumes |
| Logs | stdout se pierde | Logging driver |
| Reinicio | No | Restart policy |
| DNS | No configurado | /etc/resolv.conf inyectado |

La diferencia más importante: Docker usa **pivot_root** (escape
imposible después de desmontar oldroot) en vez de chroot, y agrega
**seccomp + capabilities** para restringir las syscalls disponibles.

---

## Agregar red al contenedor (opcional)

Docker configura la red automáticamente. Hacerlo manualmente requiere
crear un par veth y configurar NAT — exactamente lo que Docker hace:

```
Topología:
  Contenedor                     Host
  ──────────                     ────
  lo (127.0.0.1)                 lo
  veth-cont (10.0.0.2) ←──────→ veth-host (10.0.0.1)
                                  ↓
                              eth0 → Internet
                              iptables NAT (MASQUERADE)
```

```bash
# 1. Crear contenedor en background
sudo unshare --pid --fork --mount --uts --ipc --net \
    --mount-proc=/tmp/container/rootfs/proc \
    chroot /tmp/container/rootfs sleep 300 &
UNSHARE_PID=$!
sleep 1

# 2. Encontrar el PID del proceso dentro del namespace
CPID=$(pgrep -P $UNSHARE_PID)
echo "PID del contenedor en el host: $CPID"

# 3. Crear par veth
sudo ip link add veth-host type veth peer name veth-cont

# 4. Mover un extremo al namespace del contenedor
sudo ip link set veth-cont netns $CPID    # ← PID, no ruta

# 5. Configurar lado del host
sudo ip addr add 10.0.0.1/24 dev veth-host
sudo ip link set veth-host up

# 6. Configurar lado del contenedor (con nsenter)
sudo nsenter -t $CPID --net ip addr add 10.0.0.2/24 dev veth-cont
sudo nsenter -t $CPID --net ip link set veth-cont up
sudo nsenter -t $CPID --net ip link set lo up
sudo nsenter -t $CPID --net ip route add default via 10.0.0.1

# 7. Habilitar forwarding y NAT en el host
sudo sysctl -w net.ipv4.ip_forward=1
sudo iptables -t nat -A POSTROUTING -s 10.0.0.0/24 -j MASQUERADE

# 8. Probar — dentro del contenedor ahora hay red:
sudo nsenter -t $CPID --net ping -c 2 10.0.0.1     # host
sudo nsenter -t $CPID --net ping -c 2 8.8.8.8       # internet
```

### Limpieza de red

```bash
sudo kill $CPID 2>/dev/null
sudo ip link del veth-host 2>/dev/null
sudo iptables -t nat -D POSTROUTING -s 10.0.0.0/24 -j MASQUERADE 2>/dev/null
```

---

## Limpieza

```bash
# Dentro del contenedor:
exit

# En el host:
sudo rm -rf /tmp/container

# Los namespaces se destruyen automáticamente cuando el último
# proceso dentro de ellos termina — no queda rastro.
```

---

## El panorama completo de S04

```
Lo que aprendimos en esta sección:

  T01 Namespaces       → QUÉ ve el proceso
                         8 tipos de aislamiento (PID, mount, net...)

  T02 cgroups v2       → CUÁNTO puede usar
                         Límites de CPU, memoria, PIDs, I/O

  T03 chroot/pivot_root → DÓNDE vive el proceso
                         Filesystem raíz (chroot=educativo, pivot_root=producción)

  T04 Este tópico      → INTEGRACIÓN
                         Contenedor = namespaces + rootfs (+ cgroups + seguridad)

Un contenedor NO es una VM.
Un contenedor ES un proceso Linux con:
  + namespaces    → aislamiento de visibilidad
  + cgroups       → límites de recursos
  + pivot_root    → filesystem propio
  + capabilities  → privilegios reducidos
  + seccomp       → syscalls restringidas

No hay magia — son features del kernel combinadas.
```

---

## Ejercicios

### Ejercicio 1 — Las 3 piezas del contenedor actual

Verifica los tres componentes de aislamiento en el contenedor Docker
del curso.

```bash
echo "=== 1. NAMESPACES (qué VE el proceso) ==="
for ns in mnt pid net uts ipc cgroup user; do
    echo "  $ns: $(readlink /proc/self/ns/$ns 2>/dev/null || echo 'N/A')"
done

echo ""
echo "=== 2. ROOTFS (dónde VIVE el proceso) ==="
echo "  Tipo:   $(mount | grep ' / ' | awk '{print $5}')"
echo "  Distro: $(. /etc/os-release && echo $PRETTY_NAME)"
echo "  PID 1:  $(cat /proc/1/comm)"

echo ""
echo "=== 3. CGROUPS (cuánto puede USAR) ==="
echo "  Cgroup:     $(cat /proc/self/cgroup)"
echo "  memory.max: $(cat /sys/fs/cgroup/memory.max 2>/dev/null || echo 'N/A')"
echo "  cpu.max:    $(cat /sys/fs/cgroup/cpu.max 2>/dev/null || echo 'N/A')"
echo "  pids.max:   $(cat /sys/fs/cgroup/pids.max 2>/dev/null || echo 'N/A')"
```

<details><summary>Predicción</summary>

- **Namespaces**: cada uno muestra un inode (p.ej. `pid:[4026532456]`).
  Todos serán diferentes a los del host porque Docker creó namespaces
  nuevos para este contenedor. `user` puede no estar presente si Docker
  no usó user namespace.
- **Rootfs**: tipo `overlay` (overlay2 de Docker). La distro será Debian
  (el contenedor del curso). PID 1 será `bash` o el entrypoint.
- **Cgroups**: `0::/` (cgroup namespace oculta la ruta real). Los
  límites probablemente son `max` (sin restricciones configuradas).
- Este contenedor ya tiene las 3 piezas — Docker las configuró
  automáticamente.

</details>

### Ejercicio 2 — Desglose del comando unshare

Analiza qué hace cada flag y predice el resultado de omitirlo.

```bash
echo "=== Comando completo ==="
echo "sudo unshare --pid --fork --mount --uts --ipc --net \\"
echo "    --mount-proc=rootfs/proc chroot rootfs /bin/sh"
echo ""
echo "=== ¿Qué pasa si omites cada flag? ==="
echo ""
echo "Sin --pid:"
echo "  ps aux mostraría: ¿?"
echo ""
echo "Sin --fork (pero con --pid):"
echo "  echo \$\$ mostraría: ¿?"
echo ""
echo "Sin --mount:"
echo "  mount -t tmpfs tmpfs /tmp afectaría al host: ¿?"
echo ""
echo "Sin --uts:"
echo "  hostname test cambiaría el hostname del host: ¿?"
echo ""
echo "Sin --net:"
echo "  ip addr mostraría: ¿?"
echo ""
echo "Sin --mount-proc:"
echo "  ps aux mostraría: ¿?"
```

<details><summary>Predicción</summary>

- **Sin `--pid`**: `ps aux` mostraría TODOS los procesos del host.
  No hay aislamiento de PIDs.
- **Sin `--fork`** (pero con `--pid`): `echo $$` mostraría un PID
  alto (no 1). El shell está en el namespace PADRE. El nuevo PID
  namespace no tiene PID 1 → eventualmente se destruye. Es un bug
  común.
- **Sin `--mount`**: `mount -t tmpfs tmpfs /tmp` SÍ afectaría al host.
  Los mounts no estarían aislados.
- **Sin `--uts`**: `hostname test` SÍ cambiaría el hostname del host.
  Peligroso.
- **Sin `--net`**: `ip addr` mostraría las interfaces del host (eth0
  con su IP real). El contenedor compartiría la red del host (como
  `docker run --network host`).
- **Sin `--mount-proc`**: `ps aux` mostraría los procesos del host
  (el /proc viejo sigue montado). Necesitas montar un /proc nuevo
  que refleje el PID namespace nuevo.

</details>

### Ejercicio 3 — PID 1 dentro del contenedor

Investiga qué significa ser PID 1 en el namespace.

```bash
# En tu shell actual (dentro del contenedor Docker del curso):
echo "Mi PID: $$"
cat /proc/$$/comm
cat /proc/$$/status | grep -E '^(Pid|PPid|NSpid)'

# PID 1 del contenedor
cat /proc/1/comm
cat /proc/1/status | grep -E '^(Pid|PPid|NSpid)'

# ¿Qué muestra NSpid?
# ¿Es tu PID el mismo dentro y fuera del namespace?
```

<details><summary>Predicción</summary>

- `$$` muestra tu PID dentro del namespace (un número como 7 o 15).
- `/proc/$$/status` muestra `Pid:` (PID en este namespace) y `NSpid:`
  con dos valores: el PID en el namespace del host y el PID en el
  namespace del contenedor.
- PID 1 del contenedor (`/proc/1/comm`) es el entrypoint (bash o el
  proceso principal).
- `NSpid` de PID 1 mostrará algo como `NSpid: 12345  1` — donde
  12345 es su PID real en el host y 1 es su PID en el namespace del
  contenedor.
- Esto demuestra que el PID namespace es solo una **vista**: el
  proceso existe en ambos namespaces con PIDs diferentes. El kernel
  mantiene la correspondencia.

</details>

### Ejercicio 4 — Mount namespace: ¿copia o vacío?

Verifica que el mount namespace empieza como copia del padre.

```bash
# Puntos de montaje actuales
echo "Total mounts: $(mount | wc -l)"
mount | head -5

# ¿El rootfs es overlay?
mount | grep ' / '

# ¿Hay mounts del host visibles?
# (pista: /etc/resolv.conf y /etc/hostname suelen ser bind mounts)
mount | grep -E 'resolv|hostname|hosts'

# ¿Qué pasaría si montamos algo nuevo?
mount -t tmpfs tmpfs /mnt 2>/dev/null && echo "OK" || echo "FALLÓ"
mount | grep /mnt
umount /mnt 2>/dev/null

# Pregunta: ¿este mount afectó al host?
```

<details><summary>Predicción</summary>

- Habrá bastantes mounts (20-30+): overlay para /, proc, sysfs, devpts,
  tmpfs, cgroup, más bind mounts de Docker.
- `/ ` es `overlay` (overlay2 de Docker).
- `/etc/resolv.conf`, `/etc/hostname` y `/etc/hosts` son bind mounts
  desde el host — Docker los inyecta individualmente.
- Montar tmpfs en /mnt probablemente falla (el contenedor no tiene
  `CAP_SYS_ADMIN`). Si funcionara, NO afectaría al host porque
  estamos en un mount namespace aislado.
- El mount namespace empieza como una **copia** de los mounts del padre,
  pero los cambios posteriores son privados (gracias a que Docker usa
  `--make-rprivate`).

</details>

### Ejercicio 5 — Network namespace: vacío vs configurado

Compara la red del contenedor Docker con lo que tendría un namespace
vacío.

```bash
echo "=== Red de ESTE contenedor (configurada por Docker) ==="
echo "Interfaces:"
ip -brief addr
echo ""
echo "Rutas:"
ip route
echo ""
echo "DNS:"
cat /etc/resolv.conf | grep nameserver
echo ""
echo "Conectividad:"
ping -c 1 -W 1 8.8.8.8 2>/dev/null && echo "Internet: OK" || echo "Internet: NO"

echo ""
echo "=== Lo que tendría un namespace VACÍO (--net sin configurar) ==="
echo "  Solo: lo (LOOPBACK, DOWN)"
echo "  Sin rutas"
echo "  Sin DNS"
echo "  Sin conectividad"
echo ""
echo "Docker configuró:"
echo "  1. Par veth (virtual ethernet)"
echo "  2. Un extremo como eth0 en el contenedor"
echo "  3. Otro extremo conectado al bridge docker0 en el host"
echo "  4. IP asignada al contenedor"
echo "  5. Ruta default via el bridge"
echo "  6. NAT (iptables MASQUERADE) en el host"
echo "  7. DNS: el resolver del host inyectado en /etc/resolv.conf"
```

<details><summary>Predicción</summary>

- `ip -brief addr` muestra `lo` (UP, 127.0.0.1) y `eth0` (UP, con
  una IP como 172.18.0.X/16).
- `ip route` muestra una ruta default via la gateway de Docker
  (172.18.0.1) y la red local.
- DNS apunta al resolver del host o a 127.0.0.11 (Docker's embedded
  DNS).
- `ping 8.8.8.8` funciona — Docker configuró NAT.
- Un `unshare --net` sin configuración adicional NO tendría nada de
  esto: solo loopback DOWN, sin IP, sin rutas, sin conectividad.
  La sección "Agregar red" del tema explica cómo Docker hace esto
  manualmente con `ip link`, `ip addr`, e `iptables`.

</details>

### Ejercicio 6 — Capabilities: qué puede hacer el contenedor

Docker reduce las capabilities para seguridad. Investiga.

```bash
echo "=== Capabilities del proceso actual ==="
cat /proc/self/status | grep -i cap

echo ""
echo "=== Decodificar CapEff (capabilities efectivas) ==="
CAPEFF=$(cat /proc/self/status | grep CapEff | awk '{print $2}')
echo "CapEff (hex): $CAPEFF"

# Si capsh está disponible:
capsh --decode=$CAPEFF 2>/dev/null || echo "(capsh no disponible)"

echo ""
echo "=== Seccomp ==="
cat /proc/self/status | grep Seccomp
echo "  0 = disabled, 1 = strict, 2 = filter (Docker default)"

echo ""
echo "=== Intentar operaciones restringidas ==="
hostname nuevo-nombre 2>/dev/null && echo "hostname: OK" || echo "hostname: BLOQUEADO"
mount -t tmpfs tmpfs /mnt 2>/dev/null && echo "mount: OK" && umount /mnt || echo "mount: BLOQUEADO"
```

<details><summary>Predicción</summary>

- `CapEff` muestra un valor hexadecimal. Docker por defecto otorga
  ~14 de ~41 capabilities (como `CAP_CHOWN`, `CAP_NET_BIND_SERVICE`,
  `CAP_KILL`). Las peligrosas como `CAP_SYS_ADMIN`, `CAP_NET_ADMIN`,
  `CAP_SYS_PTRACE` están eliminadas.
- `capsh --decode` traduce el hex a nombres legibles.
- Seccomp probablemente muestra `2` (filter mode) — Docker bloquea
  ~50 syscalls peligrosas (reboot, swapon, clock_settime...).
- `hostname nuevo-nombre` puede funcionar si tiene `CAP_SYS_ADMIN`
  (Docker la incluye a veces con `--privileged`).
- `mount -t tmpfs` probablemente falla (necesita `CAP_SYS_ADMIN`).
- Un contenedor manual con `unshare` NO tiene estas restricciones —
  corre como root real con todas las capabilities. Docker agrega
  esta capa de seguridad adicional.

</details>

### Ejercicio 7 — Comparar desde fuera: host ve el contenedor

Desde dentro del contenedor, verifica cómo se ve desde el host.

```bash
echo "=== Lo que el host ve de este contenedor ==="
echo ""

# Nuestro PID real en el host
echo "NSpid (PID en cada namespace):"
cat /proc/self/status | grep NSpid

# Nuestro cgroup real
echo ""
echo "Cgroup (sin namespace = ruta real):"
cat /proc/1/cgroup

# ¿Cuántos procesos hay en este contenedor?
echo ""
echo "Procesos en este contenedor:"
ps -e --no-headers | wc -l

echo ""
echo "Desde el host, el administrador vería:"
echo "  - Nuestros procesos con PIDs del host (ps aux | grep ...)"
echo "  - Nuestro cgroup: /sys/fs/cgroup/system.slice/docker-<hash>.scope"
echo "  - Nuestros namespaces: /proc/<host-pid>/ns/"
echo "  - Nuestro rootfs: /var/lib/docker/overlay2/<hash>/merged/"
echo ""
echo "Un contenedor NO es invisible para el host."
echo "Es invisible solo para OTROS contenedores."
```

<details><summary>Predicción</summary>

- `NSpid` muestra dos números: el PID real en el host y el PID dentro
  del namespace. Ejemplo: `NSpid: 98765  42` — el host lo ve como
  98765, nosotros como 42.
- El cgroup muestra `0::/` (el cgroup namespace oculta la ruta real).
  En el host sería algo como
  `0::/system.slice/docker-abc123.scope`.
- Habrá unos pocos procesos en el contenedor (típicamente <10).
- El punto clave: los contenedores son transparentes para el host.
  `root` en el host puede ver todos los procesos, acceder al rootfs
  (overlay2), e inspeccionar los namespaces de cualquier contenedor.
  El aislamiento es **unidireccional**: el contenedor no ve el host,
  pero el host sí ve el contenedor.

</details>

### Ejercicio 8 — ¿Qué falta para un contenedor real?

Auditoría de seguridad: ¿qué le falta a un `unshare + chroot`?

```bash
echo "=== Checklist de un contenedor real ==="
echo ""

echo "1. Filesystem [chroot vs pivot_root]:"
echo "   Nosotros con unshare: chroot (escapable con root)"
echo "   Docker:               pivot_root (seguro, desmonta oldroot)"
echo ""

echo "2. Capabilities:"
CAPEFF=$(cat /proc/self/status | grep CapEff | awk '{print $2}')
echo "   Este contenedor: CapEff = $CAPEFF"
echo "   unshare manual: 00000003ffffffff (TODAS — peligroso)"
echo ""

echo "3. Seccomp:"
echo "   Este contenedor: Seccomp = $(cat /proc/self/status | grep Seccomp | awk '{print $2}')"
echo "   unshare manual: 0 (sin filtro — todas las syscalls permitidas)"
echo ""

echo "4. User namespace:"
echo "   Este contenedor: user ns = $(readlink /proc/self/ns/user)"
echo "   unshare manual (sin --user): root real = root real"
echo ""

echo "5. Cgroups:"
echo "   Este contenedor: $(cat /sys/fs/cgroup/memory.max 2>/dev/null || echo 'N/A')"
echo "   unshare manual: sin límites (puede consumir toda la RAM)"
echo ""

echo "=== Conclusión ==="
echo "Un unshare+chroot es un JUGUETE educativo, no un contenedor seguro."
echo "Docker agrega capas de seguridad que hacen la diferencia."
```

<details><summary>Predicción</summary>

- **Filesystem**: Docker usa pivot_root → escape imposible. unshare
  manual usa chroot → root puede escapar (T03).
- **Capabilities**: Docker reduce a ~14. unshare manual tiene TODAS
  → el proceso puede hacer `mount`, `reboot`, `ptrace`, etc.
- **Seccomp**: Docker activa filtro (modo 2) bloqueando ~50 syscalls.
  unshare manual no activa seccomp → todas las syscalls permitidas.
- **User namespace**: Docker puede usar user namespace (rootless).
  unshare sin `--user` → root dentro es root real.
- **Cgroups**: Docker configura cgroups automáticamente. unshare
  manual no pone límites → el "contenedor" puede consumir toda la
  RAM/CPU del host.
- Cada capa ausente es un vector de ataque. Un contenedor real
  necesita todas estas capas combinadas.

</details>

### Ejercicio 9 — Simular el flujo de Docker

Recorre conceptualmente los pasos que Docker ejecuta.

```bash
echo "=== Flujo de Docker al hacer 'docker run alpine sh' ==="
echo ""
echo "Paso 1: Pull/Extract image"
echo "  → Descargar capas de la imagen"
echo "  → Combinar con overlay2 (lower+upper+work)"
echo "  Verificar: $(mount | grep ' / ' | awk '{print $5}')"
echo ""

echo "Paso 2: Create namespaces"
echo "  → clone() con CLONE_NEWPID|CLONE_NEWNS|CLONE_NEWUTS|..."
echo "  Verificar:"
for ns in pid mnt net uts ipc cgroup; do
    echo "    $ns: $(readlink /proc/1/ns/$ns)"
done
echo ""

echo "Paso 3: Configure cgroup"
echo "  → mkdir /sys/fs/cgroup/docker-<hash>.scope"
echo "  → Escribir límites en memory.max, cpu.max, pids.max"
echo "  Verificar: cgroup=$(cat /proc/1/cgroup)"
echo ""

echo "Paso 4: pivot_root"
echo "  → mount --bind rootfs rootfs"
echo "  → pivot_root rootfs oldroot"
echo "  → umount -l oldroot"
echo "  Verificar: / es $(mount | grep ' / ' | awk '{print $1}')"
echo ""

echo "Paso 5: Mount /proc, /sys, /dev"
echo "  Verificar: $(mount | grep -c proc) mount(s) de proc"
echo ""

echo "Paso 6: Drop capabilities + enable seccomp"
echo "  Verificar: Seccomp=$(cat /proc/1/status | grep Seccomp | awk '{print $2}')"
echo ""

echo "Paso 7: exec(entrypoint)"
echo "  PID 1 = $(cat /proc/1/comm)"
```

<details><summary>Predicción</summary>

- **Paso 1**: El filesystem es `overlay` — Docker combinó las capas
  de la imagen Alpine.
- **Paso 2**: Cada namespace tiene un inode diferente al del host,
  confirmando que Docker creó namespaces nuevos con `clone()`.
- **Paso 3**: El cgroup muestra `0::/` (namespace oculta la ruta).
- **Paso 4**: `/` está montado como `overlay` — el pivot_root ya
  ocurrió y el oldroot fue desmontado.
- **Paso 5**: Hay al menos un mount de proc (`/proc`).
- **Paso 6**: Seccomp = 2 (filter mode activo).
- **Paso 7**: PID 1 es `bash` (o el entrypoint del contenedor del
  curso).
- Este ejercicio muestra que todo lo que vimos en S04 (namespaces,
  cgroups, pivot_root) se ejecuta en secuencia al hacer un simple
  `docker run`.

</details>

### Ejercicio 10 — Panorama: de los procesos a los contenedores

Ejercicio integrador de todo C03 (Gestión de Procesos).

```bash
echo "=== C03: De los procesos a los contenedores ==="
echo ""
echo "S01 — Inspección de Procesos"
echo "  /proc, ps, pstree, top"
echo "  Verificar: $(ls /proc/$$/comm && echo '/proc funciona')"
echo ""
echo "S02 — Control de Procesos"
echo "  Señales, fg/bg, jobs, nohup"
echo "  Verificar: PID de este shell = $$"
echo ""
echo "S03 — Procesos Especiales"
echo "  Zombies, PID 1, daemons"
echo "  Verificar: PID 1 = $(cat /proc/1/comm)"
echo ""
echo "S04 — Aislamiento de Procesos"
echo "  Namespaces + cgroups + chroot/pivot_root = contenedor"
echo "  Verificar:"
echo "    Namespace PID: $(readlink /proc/self/ns/pid)"
echo "    Cgroup: $(cat /proc/self/cgroup)"
echo "    Rootfs: $(mount | grep ' / ' | awk '{print $5}')"
echo ""
echo "=== Conclusión ==="
echo "Un contenedor es un PROCESO con aislamiento."
echo "Todo lo que aprendimos sobre procesos (inspección, señales,"
echo "PID 1, daemons) aplica DENTRO del contenedor."
echo "La diferencia: los namespaces limitan lo que ve,"
echo "los cgroups limitan lo que usa, y pivot_root le da"
echo "su propio filesystem."
echo ""
echo "Próximo: B06/C09 → implementar esto en C con clone(),"
echo "pivot_root() y la API de cgroups v2."
```

<details><summary>Predicción</summary>

- Este ejercicio conecta los cuatro temas de C03:
  - S01: `/proc` sigue siendo la fuente de información, incluso dentro
    de contenedores (pero filtrada por el PID namespace).
  - S02: Las señales funcionan igual dentro del contenedor. `kill`,
    `fg`, `bg` operan dentro del PID namespace.
  - S03: PID 1 del contenedor tiene las mismas responsabilidades que
    PID 1 del host (reaping de zombies, manejo de señales). Por eso
    Docker ofrece `--init` (tini).
  - S04: Los tres mecanismos (namespaces, cgroups, pivot_root) se
    combinan para crear un contenedor.
- El rootfs es `overlay`, el namespace PID tiene un inode único,
  el cgroup es `0::/`.
- La conclusión clave: un contenedor no es magia — es la aplicación
  combinada de todo lo que aprendimos sobre procesos Linux.

</details>
