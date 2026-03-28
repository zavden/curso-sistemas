# T03 — Capabilities y seccomp

## Linux capabilities

Históricamente, Linux tenía un modelo binario de privilegios: o eres root (puedes
todo) o no eres root (no puedes casi nada). Desde el kernel 2.2, los privilegios
de root se dividieron en **capabilities** — permisos granulares independientes.

Un proceso puede tener solo las capabilities que necesita, sin tener root completo.
Docker usa este mecanismo para limitar lo que un contenedor puede hacer.

## Capabilities por defecto en Docker

Docker mantiene un subconjunto de capabilities para que la mayoría de aplicaciones
funcionen. Las capabilities se dividen en tres categorías según su riesgo:

### Capabilities que Docker MANTIENE por defecto

| Capability | Qué permite | Por qué se mantiene |
|---|---|---|
| `CHOWN` | Cambiar propietario de archivos | Necesario para muchos instaladores |
| `DAC_OVERRIDE` | Ignorar permisos de lectura/escritura | Permite a root leer/escribir cualquier archivo |
| `FOWNER` | Bypass de checks de propietario | Operaciones sobre archivos de otros |
| `FSETID` | Mantener setuid/setgid al modificar archivos | Compatibilidad con aplicaciones |
| `KILL` | Enviar señales a cualquier proceso | Gestión de procesos |
| `SETGID` | Cambiar GID del proceso | Necesario para drop de privilegios |
| `SETUID` | Cambiar UID del proceso | Necesario para drop de privilegios |
| `SETPCAP` | Modificar capabilities del proceso | Gestión de capabilities |
| `NET_BIND_SERVICE` | Bindear puertos < 1024 | Servidores web, mail, etc. |
| `NET_RAW` | Sockets raw (ping, traceroute) | Diagnóstico de red |
| `SYS_CHROOT` | Usar chroot | Algunas aplicaciones lo requieren |
| `MKNOD` | Crear device nodes | Compatibilidad |
| `AUDIT_WRITE` | Escribir al log de auditoría | Logging |
| `SETFCAP` | Establecer file capabilities | Gestión de capabilities en archivos |

### Capabilities que Docker ELIMINA por defecto

| Capability | Qué permite | Por qué se elimina |
|---|---|---|
| `SYS_ADMIN` | mount, pivot_root, BPF, namespaces | **La más peligrosa** — permite escapar |
| `NET_ADMIN` | Configurar interfaces, firewall, routing | Manipulación de la red del host |
| `SYS_PTRACE` | Trazar procesos (ptrace, strace) | Inspeccionar otros contenedores |
| `SYS_MODULE` | Cargar módulos del kernel | Acceso directo al kernel |
| `SYS_RAWIO` | Acceso a puertos I/O (ioperm, iopl) | Acceso al hardware |
| `SYS_TIME` | Cambiar el reloj del sistema | Afecta a todo el host |
| `SYS_BOOT` | Rebootear el sistema | Afecta al host |
| `MAC_ADMIN` | Configurar SELinux/AppArmor | Debilitar seguridad del host |
| `SYSLOG` | Operar con el kernel syslog | Información sensible |

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
```

### Añadir capabilities (`--cap-add`)

```bash
# Añadir SYS_TIME: permite cambiar el reloj
docker run --rm --cap-add SYS_TIME alpine date -s "2025-01-01"
# Si funciona, el reloj del HOST cambia (¡cuidado!)

# Añadir SYS_PTRACE: permite strace
docker run --rm --cap-add SYS_PTRACE alpine sh -c \
  'apk add --no-cache strace > /dev/null 2>&1 && strace -c ls / 2>&1 | tail -5'
```

### Patrón recomendado: drop ALL + add solo lo necesario

```bash
# Eliminar TODAS las capabilities y añadir solo las necesarias
docker run --rm \
  --cap-drop ALL \
  --cap-add NET_BIND_SERVICE \
  nginx

# El contenedor solo puede bindear puertos privilegiados
# No puede: chown, kill otros procesos, crear devices, etc.
```

Este es el **principio de mínimo privilegio** aplicado a capabilities. Cada
capability adicional es superficie de ataque.

### Listar capabilities de un contenedor

```bash
docker run --rm alpine sh -c 'cat /proc/1/status | grep Cap'
# CapInh: 0000000000000000
# CapPrm: 00000000a80425fb
# CapEff: 00000000a80425fb
# CapBnd: 00000000a80425fb
# CapAmb: 0000000000000000

# Decodificar el bitmask (en el host con capsh instalado)
capsh --decode=00000000a80425fb
# 0x00000000a80425fb=cap_chown,cap_dac_override,cap_fowner,...
```

```bash
# Con --cap-drop ALL
docker run --rm --cap-drop ALL alpine sh -c 'cat /proc/1/status | grep CapEff'
# CapEff: 0000000000000000  — sin capabilities
```

## seccomp — Filtrado de syscalls

seccomp (Secure Computing Mode) opera a un nivel más bajo que las capabilities.
Mientras las capabilities controlan **qué privilegios** tiene un proceso, seccomp
controla **qué syscalls** puede ejecutar.

Docker aplica un perfil seccomp por defecto que bloquea ~44 syscalls peligrosas.

### Syscalls bloqueadas por defecto

| Syscall | Qué hace | Riesgo |
|---|---|---|
| `mount` / `umount2` | Montar/desmontar filesystems | Escapar del contenedor |
| `reboot` | Reiniciar el sistema | Afecta al host |
| `kexec_load` | Cargar un nuevo kernel | Reemplazar el kernel |
| `init_module` | Cargar módulo del kernel | Código en kernel space |
| `pivot_root` | Cambiar el root filesystem | Escapar del contenedor |
| `swapon` / `swapoff` | Gestionar swap | Afecta rendimiento del host |
| `clock_settime` | Cambiar el reloj | Afecta a todo el host |
| `acct` | Activar process accounting | Información sensible |
| `settimeofday` | Cambiar hora del sistema | Afecta al host |
| `unshare` | Crear nuevos namespaces | Escapar del aislamiento |

### Verificar seccomp

```bash
# Verificar que seccomp está activo
docker info | grep -i seccomp
# seccomp: Profile: builtin

# Un contenedor por defecto no puede usar mount
docker run --rm alpine mount -t tmpfs none /tmp 2>&1
# mount: permission denied (¿are you root?)
# → seccomp bloquea la syscall mount
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
```

### Perfiles seccomp custom

Se puede crear un perfil JSON que permita o bloquee syscalls específicas:

```json
{
  "defaultAction": "SCMP_ACT_ERRNO",
  "architectures": ["SCMP_ARCH_X86_64"],
  "syscalls": [
    {
      "names": ["read", "write", "open", "close", "stat", "fstat",
                "mmap", "mprotect", "munmap", "brk", "exit_group",
                "execve", "arch_prctl", "access", "getpid"],
      "action": "SCMP_ACT_ALLOW"
    }
  ]
}
```

```bash
# Usar un perfil custom
docker run --rm --security-opt seccomp=/path/to/profile.json alpine ls
```

En la práctica, el perfil por defecto de Docker es adecuado para la mayoría de
aplicaciones. Solo se necesitan perfiles custom en casos muy específicos (debugging
de kernel, herramientas de tracing).

## `--privileged` — La opción nuclear

`--privileged` **desactiva todas las restricciones de seguridad**:

```
--privileged equivale a:
  ✗ Todas las capabilities activadas
  ✗ seccomp desactivado
  ✗ AppArmor/SELinux desactivado
  ✗ Acceso a todos los devices del host (/dev/*)
  ✗ Puede montar filesystems
  ✗ Puede cargar módulos del kernel
```

```bash
# Con --privileged, el contenedor puede hacer casi todo lo que root puede en el host
docker run --rm --privileged alpine sh -c \
  'mount -t tmpfs none /mnt && ls /dev | wc -l'
# mount funciona, acceso a todos los devices

# Puede ver los discos del host
docker run --rm --privileged alpine fdisk -l 2>/dev/null | head -5
```

### Cuándo es legítimo usar `--privileged`

| Caso | Por qué |
|---|---|
| Docker-in-Docker (dind) | Necesita montar overlayfs, crear namespaces |
| Acceso a hardware (GPU, USB) | Necesita `/dev/*` |
| Herramientas de debugging del kernel | strace, perf, bpf |
| Builds de imágenes con Buildkit | Algunas operaciones de build requieren mount |

En **todos los demás casos**, usar `--cap-add` con las capabilities específicas
es preferible a `--privileged`.

## Capas de seguridad combinadas

Docker aplica múltiples capas de seguridad simultáneamente:

```
┌─────────────────────────────────────────┐
│              Aplicación                 │
├─────────────────────────────────────────┤
│  USER non-root        (usuario)         │
├─────────────────────────────────────────┤
│  Capabilities         (permisos)        │
├─────────────────────────────────────────┤
│  seccomp              (syscalls)        │
├─────────────────────────────────────────┤
│  AppArmor / SELinux   (MAC)             │
├─────────────────────────────────────────┤
│  Namespaces           (aislamiento)     │
├─────────────────────────────────────────┤
│  cgroups              (recursos)        │
├─────────────────────────────────────────┤
│              Kernel Linux               │
└─────────────────────────────────────────┘
```

Cada capa es independiente. Un fallo en una no compromete las demás (defensa en
profundidad). La configuración más segura combina: non-root + cap-drop ALL +
cap-add mínimo + seccomp default + SELinux/AppArmor.

---

## Ejercicios

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
```
