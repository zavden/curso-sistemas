# T03 — Capabilities y seccomp

## Linux capabilities

Históricamente, Linux tenía un modelo binario de privilegios: o eres root (puedes
todo) o no eres root (no puedes casi nada). Desde el kernel 2.2, los privilegios
de root se dividieron en **capabilities** — permisos granulares independientes.

Un proceso puede tener solo las capabilities que necesita, sin tener root completo.

```bash
# Ver todas las capabilities disponibles en el kernel
capsh --print | grep "Current:"
# Current: = cap_chown,...,cap_sys_resource+eip
# Significa que este proceso tiene estas capabilities activas
```

### Lista completa de capabilities

Hay más de 40 capabilities en Linux. Las más relevantes:

```
CAP_CHOWN           Cambiar dueño/grupo de archivos
CAP_DAC_OVERRIDE    Ignorar permisos de archivo (DAC)
CAP_DAC_READ_SEARCH Ignorar permisos de lectura/búsqueda
CAP_FOWNER          Bypass de checks de propietario
CAP_FSETID          Mantener setuid/setgid al modificar archivos
CAP_KILL            Enviar señales a cualquier proceso
CAP_SETGID          Cambiar GID del proceso
CAP_SETUID          Cambiar UID del proceso
CAP_SETPCAP         Modificar capabilities del proceso
CAP_NET_BIND_SERVICE Bind a puertos privilegiados (<1024)
CAP_NET_BROADCAST   Broadcasting de red
CAP_NET_ADMIN       Administración de red (ifconfig, iptables, routing)
CAP_NET_RAW         Raw sockets (ping, traceroute)
CAP_IPC_LOCK        Bloquear memoria en RAM (mlock)
CAP_IPC_OWNER       Operaciones IPC (semaphores, shared memory)
CAP_SYS_MODULE      Cargar módulos del kernel
CAP_SYS_RAWIO      Acceso directo a I/O (ioperm, iopl)
CAP_SYS_CHROOT      Usar chroot()
CAP_SYS_ADMIN       Administrator (la más amplia) - mount, pivot_root, etc.
CAP_SYS_BOOT        Rebootear
CAP_SYS_NICE        Cambiar prioridad de procesos
CAP_SYS_RESOURCE    Sobrescribir resource limits
CAP_SYS_TIME        Cambiar hora del sistema
CAP_SYS_TTY_CONFIG  Configurar TTY
CAP_MKNOD           Crear device nodes
CAP_LEASE           Establecer leases en archivos
CAP_AUDIT_WRITE     Escribir al log de auditoría
CAP_AUDIT_CONTROL   Controlar auditoría del sistema
CAP_SETFCAP         Establecer file capabilities
CAP_MAC_OVERRIDE    Ignorar Mandatory Access Control (MAC)
CAP_MAC_ADMIN       Configurar MAC (SELinux/AppArmor)
CAP_SYSLOG          Operaciones con syslog
CAP_WAKE_ALARM      Configurar wakeup alarms
CAP_BLOCK_SUSPEND   Prevenir suspend del sistema
CAP_AUDIT_READ      Leer el log de auditoría
```

## Capabilities por defecto en Docker

Docker mantiene un subconjunto de capabilities para que la mayoría de aplicaciones
funcionen. Las capabilities se dividen en tres categorías según su riesgo.

### Capabilities que Docker MANTIENE por defecto

| Capability | Qué permite | Por qué se mantiene |
|---|---|---|
| `CHOWN` | Cambiar propietario de archivos | Necesario para muchos instaladores |
| `DAC_OVERRIDE` | Ignorar permisos de lectura/escritura | Permite a root leer/escribir cualquier archivo |
| `FOWNER` | Bypass de checks de propietario | Operaciones sobre archivos de otros usuarios |
| `FSETID` | Mantener setuid/setgid al modificar archivos | Compatibilidad con aplicaciones |
| `KILL` | Enviar señales a cualquier proceso | Gestión de procesos |
| `SETGID` | Cambiar GID del proceso | Necesario para drop de privilegios |
| `SETUID` | Cambiar UID del proceso | Necesario para drop de privilegios |
| `SETPCAP` | Modificar capabilities del proceso | Gestión de capabilities |
| `NET_BIND_SERVICE` | Bindear puertos < 1024 | Servidores web, mail, etc. |
| `NET_RAW` | Sockets raw (ping, traceroute) | Diagnóstico de red |
| `SYS_CHROOT` | Usar chroot | Algunas aplicaciones lo requieren |
| `MKNOD` | Crear device nodes | Compatibilidad con aplicaciones legacy |
| `AUDIT_WRITE` | Escribir al log de auditoría | Logging |
| `SETFCAP` | Establecer file capabilities | Gestión de capabilities en archivos |

### Capabilities que Docker ELIMINA por defecto

| Capability | Qué permite | Por qué se elimina |
|---|---|---|
| `SYS_ADMIN` | mount, pivot_root, BPF, namespaces, sethostname | **La más peligrosa** — permite escapar |
| `NET_ADMIN` | Configurar interfaces, firewall, routing | Manipulación de la red del host |
| `SYS_PTRACE` | Trazar procesos (ptrace, strace) | Inspeccionar otros contenedores |
| `SYS_MODULE` | Cargar módulos del kernel | Acceso directo al kernel |
| `SYS_RAWIO` | Acceso a puertos I/O (ioperm, iopl) | Acceso al hardware |
| `SYS_TIME` | Cambiar el reloj del sistema | Afecta a todo el host |
| `SYS_BOOT` | Rebootear el sistema | Afecta al host |
| `MAC_ADMIN` | Configurar SELinux/AppArmor | Debilitar seguridad del host |
| `SYSLOG` | Operar con el kernel syslog | Información sensible |
| `DAC_READ_SEARCH` | Leer cualquier archivo | Equivale a CAP_DAC_OVERRIDE |
| `SYS_RESOURCE` | Sobrescribir resource limits | Puede consumir recursos ilimitados |
| `IPC_LOCK` | Bloquear memoria (mlock) | Puede agotar memoria |
| `BLOCK_SUSPEND` | Prevenir suspend | Puede afectar al host |

## Manipular capabilities

### Eliminar capabilities (`--cap-drop`)

```bash
# Eliminar NET_RAW: ping deja de funcionar
docker run --rm alpine ping -c 1 8.8.8.8
# PING 8.8.8.8: 64 bytes from 8.8.8.8  ← funciona

docker run --rm --cap-drop NET_RAW alpine ping -c 1 8.8.8.8
# ping: permission denied (are you root?)  ← bloqueado

# Eliminar CHOWN: no se puede cambiar propietario
docker run --rm --cap-drop CHOWN alpine sh -c 'touch /tmp/test && chown nobody /tmp/test'
# chown: /tmp/test: Operation not permitted

# Eliminar SETUID: no se puede cambiar UID (su, sudo)
docker run --rm --cap-drop SETUID alpine su -s /bin/sh nobody -c 'id' 2>&1
# su: can't set uid
```

### Añadir capabilities (`--cap-add`)

```bash
# Añadir SYS_TIME: permite cambiar el reloj
docker run --rm --cap-add SYS_TIME alpine date -s "2025-01-01"
# Si funciona, el reloj del HOST cambia (¡cuidado!)

# Añadir SYS_PTRACE: permite strace
docker run --rm --cap-add SYS_PTRACE alpine sh -c \
  'apk add --no-cache strace > /dev/null 2>&1 && strace -c ls / 2>&1 | tail -5'

# Añadir NET_ADMIN: permite configurar red
docker run --rm --cap-add NET_ADMIN alpine ip link add dummy0 type dummy 2>&1
# Puede añadir interfaces de red

# Añadir SYS_ADMIN: permite mount (¡MUY peligroso!)
docker run --rm --cap-add SYS_ADMIN alpine mount -t tmpfs none /mnt 2>&1
# Funciona — esto es muy peligroso, no hacer en producción
```

### Patrón recomendado: drop ALL + add solo lo necesario

```bash
# Eliminar TODAS las capabilities y añadir solo las necesarias
docker run --rm \
  --cap-drop ALL \
  --cap-add NET_BIND_SERVICE \
  nginx

# El contenedor solo puede:
# - bindear puertos privilegiados (80, 443)
# - NADA más

# Verificar capabilities activas
docker run --rm \
  --cap-drop ALL \
  --cap-add NET_BIND_SERVICE \
  alpine sh -c 'cat /proc/1/status | grep Cap'
# CapInh: 0000000000000000
# CapPrm: 00000000a00425fb  ← solo NET_BIND_SERVICE
# CapEff: 00000000a00425fb
```

Este es el **principio de mínimo privilegio** aplicado a capabilities. Cada
capability adicional es superficie de ataque.

### Listar capabilities de un contenedor

```bash
docker run --rm alpine sh -c 'cat /proc/1/status | grep Cap'
# CapInh: 0000000000000000
# CapPrm: 00000000a80425fb  ← capabilities permitidas
# CapEff: 00000000a80425fb  ← capabilities efectivas
# CapBnd: 00000000a80425fb  ← bounding set
# CapAmb: 0000000000000000  ← ambient (heredadas)

# Decodificar el bitmask (en el host con capsh instalado)
capsh --decode=00000000a80425fb
# 0x00000000a80425fb=cap_chown,cap_dac_override,cap_fowner,...
```

Significado de cada campo:

| Campo | Significado |
|-------|-------------|
| `CapInh` | Heredadas del proceso padre |
| `CapPrm` | Capabilities permitidas (las que puede usar) |
| `CapEff` | Capabilities efectivas (las que está usando actualmente) |
| `CapBnd` | Bounding set (límite máximo de capabilities) |
| `CapAmb` | Ambient (heredadas en execve) |

```bash
# Con --cap-drop ALL
docker run --rm --cap-drop ALL alpine sh -c 'cat /proc/1/status | grep CapEff'
# CapEff: 0000000000000000  — sin capabilities

# Ver capabilities como números
docker run --rm alpine sh -c 'cat /proc/1/status | grep Cap'
capsh --print | head -5
```

## seccomp — Filtrado de syscalls

seccomp (Secure Computing Mode) opera a un nivel más bajo que las capabilities.
Mientras las capabilities controlan **qué privilegios** tiene un proceso, seccomp
controla **qué syscalls** puede ejecutar.

```
                    CAPABILITIES                    SECCOMP
                    ────────────                    ───────
¿Qué permisos       ¿Qué syscalls                  ¿Qué syscalls
 tiene el           puede ejecutar?                puede ejecutar
 proceso?                                         un proceso?

read()  ──────────→ DAC_OVERRIDE
write() ──────────→ DAC_OVERRIDE
mount() ──────────→ SYS_ADMIN ────── seccomp ──── BLOCKED
ptrace()───────────→ SYS_PTRACE ──── seccomp ──── BLOCKED
```

Docker aplica un perfil seccomp por defecto que bloquea ~44 syscalls peligrosas.

### Syscalls bloqueadas por defecto

| Syscall | Qué hace | Riesgo |
|---|---|---|
| `mount` / `umount2` | Montar/desmontar filesystems | Escapar del contenedor |
| `reboot` | Reiniciar el sistema | Afecta al host |
| `kexec_load` | Cargar un nuevo kernel | Reemplazar el kernel |
| `init_module` | Cargar módulo del kernel | Código en kernel space |
| `finit_module` | Cargar módulo del kernel (variant) | Código en kernel space |
| `pivot_root` | Cambiar el root filesystem | Escapar del contenedor |
| `swapon` / `swapoff` | Gestionar swap | Afecta rendimiento del host |
| `clock_settime` | Cambiar el reloj | Afecta a todo el host |
| `acct` | Activar process accounting | Información sensible |
| `settimeofday` | Cambiar hora del sistema | Afecta al host |
| `unshare` | Crear nuevos namespaces | Escapar del aislamiento |
| `mount` | Montar filesystems | Escapar del contenedor |
| `umount2` | Desmontar filesystems | Manipulación |
| `quotactl` | Configurar cuotas de disco | Información sensible |
| `nfsservctl` | Control del subsystema NFS | Afecta a NFS |
| `clone3` | Crear procesos con namespaces (futuro) | Aislamiento |

### Verificar seccomp

```bash
# Verificar que seccomp está activo
docker info | grep -i seccomp
# seccomp: Profile: builtin
# Si dice "unconfined", seccomp está desactivado

# Un contenedor por defecto no puede usar mount
docker run --rm alpine mount -t tmpfs none /tmp 2>&1
# mount: permission denied (¿are you root?)
# → seccomp bloquea la syscall mount

# Ver qué perfil seccomp usa Docker
docker info | grep -i "seccomp"
# Seccomp: 6
# Kernel de Linux soporta 6 perfiles seccomp
# Los perfiles se cargan desde /proc/sys/kernel/seccomp
```

### Deshabilitar seccomp (no recomendado)

```bash
# seccomp=unconfined permite TODAS las syscalls
docker run --rm --security-opt seccomp=unconfined alpine mount -t tmpfs none /mnt
# Funciona — pero el contenedor puede ejecutar cualquier syscall

# Verificar que mount funciona sin seccomp
docker run --rm --security-opt seccomp=unconfined alpine sh -c \
  'mount -t tmpfs none /mnt && df -h /mnt'
# tmpfs  ...  /mnt  — montado exitosamente

# Peligro: un contenedor sin seccomp puede:
# - Montar archivosystems
# - Modificar horarios
# - Cargar módulos del kernel
# - Rebootear el sistema
```

### Perfiles seccomp custom

Se puede crear un perfil JSON que permita o bloquee syscalls específicas:

```json
{
  "defaultAction": "SCMP_ACT_ERRNO",
  "architectures": ["SCMP_ARCH_X86_64", "SCMP_ARCH_X86"],
  "syscalls": [
    {
      "names": ["read", "write", "open", "close", "stat", "fstat",
                "mmap", "mprotect", "munmap", "brk", "exit_group",
                "execve", "arch_prctl", "access", "getpid"],
      "action": "SCMP_ACT_ALLOW"
    },
    {
      "names": ["socket", "bind", "listen", "accept", "connect"],
      "action": "SCMP_ACT_ALLOW"
    }
  ]
}
```

```bash
# Usar un perfil custom
docker run --rm --security-opt seccomp=/path/to/profile.json alpine ls

# Perfil que solo permite networking básico
# Bloquea todo lo demás (mount, admin syscalls, etc.)
```

El perfil por defecto de Docker está en:
```
/usr/share/container/seccomp.json
```

Para ver el perfil por defecto:

```bash
cat /usr/share/container/seccomp.json | jq '.syscalls | length'
# ~44 syscalls bloqueadas

cat /usr/share/container/seccomp.json | jq '.syscalls[] | select(.action == "SCMP_ACT_ERRNO") | .names'
```

## AppArmor y SELinux

Además de capabilities y seccomp, Docker puede usar MAC (Mandatory Access Control):

### AppArmor (Ubuntu/Debian)

```bash
# Ver el perfil AppArmor de un contenedor
docker inspect --format '{{.AppArmorProfile}}' container_name

# Correr con perfil específico
docker run --rm --security-opt apparmor=unconfined alpine sh

# Perfil custom
docker run --rm --security-opt apparmor=/path/to/profile myapp
```

### SELinux (RHEL/CentOS)

```bash
# Ver el contexto SELinux de un contenedor
docker inspect --format '{{.SecurityOptions}}' container_name

# Modos de SELinux para contenedores
docker run --rm --security-opt label=disable alpine sh
# Desactivar SELinux para el contenedor

# Niveles de MCS
docker run --rm --security-opt label=level:s0:c100,c200 alpine sh
```

## `--privileged` — La opción nuclear

`--privileged` **desactiva todas las restricciones de seguridad**:

```
--privileged equivale a:
  ✗ Todas las capabilities activadas (CAP_SYS_ADMIN, CAP_NET_ADMIN, etc.)
  ✗ seccomp desactivado (todas las syscalls permitidas)
  ✗ AppArmor/SELinux desactivado
  ✗ Acceso a todos los devices del host (/dev/*)
  ✗ Puede montar filesystems
  ✗ Puede cargar módulos del kernel
  ✗ Puede cambiar hostname, timezone, etc.
```

```bash
# Con --privileged, el contenedor puede hacer casi todo lo que root puede en el host
docker run --rm --privileged alpine sh -c \
  'mount -t tmpfs none /mnt && ls /dev | wc -l'
# mount funciona, acceso a todos los devices (~200+)

# Puede ver los discos del host
docker run --rm --privileged alpine fdisk -l 2>/dev/null | head -10

# Puede cargar módulos del kernel
docker run --rm --privileged alpine modprobe dummy 2>&1
# Funciona
```

### Cuándo es legítimo usar `--privileged`

| Caso | Por qué |
|---|---|
| Docker-in-Docker (dind) | Necesita montar overlayfs, crear namespaces |
| Acceso a hardware (GPU, USB) | Necesita `/dev/*` específico |
| Herramientas de debugging del kernel | strace, perf, bpf, etc. |
| Builds de imágenes con Buildkit | Algunas operaciones de build requieren mount |
| Contenedores de seguridad especiales | Contenedores que monitorizan el host |

En **todos los demás casos**, usar `--cap-add` con las capabilities específicas
es preferible a `--privileged`.

### Alternativa a `--privileged`: `--device`

```bash
# Solo exponer dispositivos específicos, no todos
docker run --rm --device=/dev/fuse alpine ls /dev/fuse
# Solo /dev/fuse está disponible

# Comparar con --privileged
docker run --rm --privileged alpine ls /dev | wc -l
# ~200 devices

docker run --rm --device=/dev/fuse alpine ls /dev | wc -l
# ~5 devices
```

## Capas de seguridad combinadas

Docker aplica múltiples capas de seguridad simultáneamente:

```
┌─────────────────────────────────────────┐
│              Aplicación                 │
├─────────────────────────────────────────┤
│  USER non-root        (usuario)          │
│  ───────────────────────────────────────│
│  Capabilities         (permisos)         │
│  ¿Qué privilegios tiene?                │
│  ───────────────────────────────────────│
│  seccomp              (syscalls)        │
│  ¿Qué syscalls puede llamar?            │
│  ───────────────────────────────────────│
│  AppArmor / SELinux   (MAC)             │
│  Mandatory Access Control                │
│  ───────────────────────────────────────│
│  Namespaces           (aislamiento)     │
│  PID, Network, Mount, User, etc.        │
│  ───────────────────────────────────────│
│  cgroups              (recursos)       │
│  CPU, memoria, I/O, PIDs               │
├─────────────────────────────────────────┤
│              Kernel Linux               │
└─────────────────────────────────────────┘
```

Cada capa es independiente. Un fallo en una no compromete las demás (defensa en
profundidad).

### Configuración más segura posible

```bash
docker run -d \
  --user 1000:1000 \
  --cap-drop ALL \
  --cap-add NET_BIND_SERVICE \
  --security-opt no-new-privileges:true \
  --read-only \
  --tmpfs /tmp:size=64m,mode=1777 \
  --mount type=volume,source=appdata,target=/data \
  myapp
```

## Práctica

### Ejercicio 1 — Eliminar capabilities una a una

```bash
# Verificar que ping funciona por defecto
docker run --rm alpine ping -c 1 127.0.0.1

# Eliminar NET_RAW → ping falla
docker run --rm --cap-drop NET_RAW alpine ping -c 1 127.0.0.1 2>&1
# ping: permission denied

# Eliminar CHOWN → chown falla
docker run --rm --cap-drop CHOWN alpine sh -c 'touch /tmp/f && chown 1000 /tmp/f' 2>&1
# chown: Operation not permitted

# Eliminar SETUID → su/gosu falla
docker run --rm --cap-drop SETUID alpine su -s /bin/sh nobody -c 'id' 2>&1
# su: can't set uid

# Ver las capabilities activas
docker run --rm alpine cat /proc/1/status | grep Cap
```

### Ejercicio 2 — cap-drop ALL + añadir lo necesario

```bash
# Sin capabilities: casi nada funciona
docker run --rm --cap-drop ALL alpine sh -c 'ping -c1 127.0.0.1 2>&1; chown nobody /tmp 2>&1'
# Ambos fallan

# Añadir solo NET_RAW
docker run --rm --cap-drop ALL --cap-add NET_RAW alpine ping -c 1 127.0.0.1
# ping funciona, chown sigue fallando

# Añadir NET_RAW + CHOWN
docker run --rm --cap-drop ALL --cap-add NET_RAW --cap-add CHOWN alpine sh -c \
  'ping -c1 127.0.0.1 && touch /tmp/f && chown nobody /tmp/f && echo "OK"'
```

### Ejercicio 3 — Comparar normal vs privileged

```bash
# Normal: mount no funciona
docker run --rm alpine mount -t tmpfs none /mnt 2>&1
# mount: permission denied

# Privileged: mount funciona
docker run --rm --privileged alpine sh -c \
  'mount -t tmpfs none /mnt && echo "mount OK" && ls /dev | wc -l'
# mount OK
# ~200+ devices visibles

# Normal: devices limitados
docker run --rm alpine ls /dev | wc -l
# ~15 devices

# Comparar capabilities
docker run --rm --privileged alpine cat /proc/1/status | grep CapEff
# 0000003fffffffff  ← TODAS las capabilities
```

### Ejercicio 4 — Decodificar capabilities

```bash
# Ver el bitmask de capabilities
docker run --rm alpine cat /proc/1/status | grep Cap
# CapPrm: 00000000a80425fb

# Decodificar en el host
capsh --decode=00000000a80425fb

# Comparar con --cap-drop ALL
docker run --rm --cap-drop ALL alpine cat /proc/1/status | grep CapEff
# CapEff: 0000000000000000

# Comparar con --privileged
docker run --rm --privileged alpine cat /proc/1/status | grep CapEff
# CapEff: 0000003fffffffff  ← TODAS
```

### Ejercicio 5 — Seccomp y syscalls

```bash
# Normal: mount bloqueado por seccomp
docker run --rm alpine sh -c 'mount -t tmpfs none /mnt' 2>&1
# mount: permission denied

# Con seccomp=unconfined: mount funciona
docker run --rm --security-opt seccomp=unconfined alpine sh -c 'mount -t tmpfs none /mnt && echo OK'

# Ver el perfil seccomp de Docker
cat /usr/share/container/seccomp.json | jq '.syscalls[] | select(.names | index("mount"))'
```

### Ejercicio 6 — Perfil seccomp custom

```bash
# Crear un perfil seccomp minimal
cat > /tmp/custom-seccomp.json << 'EOF'
{
  "defaultAction": "SCMP_ACT_ERRNO",
  "architectures": ["SCMP_ARCH_X86_64"],
  "syscalls": [
    {
      "names": ["read", "write", "exit_group", "execve"],
      "action": "SCMP_ACT_ALLOW"
    }
  ]
}
EOF

# Probar con el perfil custom
docker run --rm --security-opt seccomp=/tmp/custom-seccomp.json alpine ls 2>&1
# Puede fallar porque falta open, close, etc.

# Agregar más syscalls
cat > /tmp/permissive-seccomp.json << 'EOF'
{
  "defaultAction": "SCMP_ACT_ALLOW",
  "architectures": ["SCMP_ARCH_X86_64"],
  "syscalls": [
    {
      "names": ["mount", "umount2"],
      "action": "SCMP_ACT_ERRNO"
    }
  ]
}
EOF

# Este perfil permite todo EXCEPTO mount
docker run --rm --security-opt seccomp=/tmp/permissive-seccomp.json alpine ls
# Funciona

docker run --rm --security-opt seccomp=/tmp/permissive-seccomp.json alpine sh -c 'mount -t tmpfs none /mnt' 2>&1
# Bloqueado

rm /tmp/custom-seccomp.json /tmp/permissive-seccomp.json
```

### Ejercicio 7 — --device vs --privileged

```bash
# Con --device puedes exponer solo ciertos devices
docker run --rm --device=/dev/null alpine ls -la /dev/null
# crw-rw-rw- 1 root root 1, 3 ...

docker run --rm --device=/dev/fuse alpine ls /dev/fuse
# crw------- 1 root root 10, 229 ...

# Pero no puedes montar
docker run --rm --device=/dev/fuse alpine sh -c 'mount --bind /dev /mnt' 2>&1
# mount: permission denied

# Privileged expone todos los devices Y permite mount
docker run --rm --privileged --device=/dev/fuse alpine sh -c 'ls /dev | wc -l'
# ~200 devices

rm /tmp/*.json 2>/dev/null
```

### Ejercicio 8 — Combinar múltiples capas de seguridad

```bash
# Aplicar múltiples restricciones
docker run -d \
  --name secure-container \
  --user 1000:1000 \
  --cap-drop ALL \
  --cap-add NET_BIND_SERVICE \
  --security-opt no-new-privileges:true \
  --read-only \
  --tmpfs /tmp:size=64m,mode=1777 \
  alpine sleep 300

# Ver las restricciones
docker inspect secure-container --format '{{json .HostConfig}}' | jq '{User: .UsernsMode, Caps: .CapDrop, AddCaps: .CapAdd, Readonly: .ReadonlyRootfs}'

# Ver que funciona el bind a puerto privilegiado
docker exec secure-container id
# uid=1000

docker exec secure-container cat /proc/1/status | grep CapEff
# 00000000a00425fb ← solo NET_BIND_SERVICE

# Cleanup
docker rm -f secure-container
```
