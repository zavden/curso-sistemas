# T03 — Capabilities y seccomp

## Linux capabilities

Históricamente, Linux tenía un modelo binario de privilegios: o eres root (puedes
todo) o no eres root (no puedes casi nada). Desde el kernel 2.2, los privilegios
de root se dividieron en **capabilities** — permisos granulares independientes.

Un proceso puede tener solo las capabilities que necesita, sin tener root completo.
Docker usa este mecanismo para limitar lo que un contenedor puede hacer.

## Capabilities por defecto en Docker

Docker mantiene un subconjunto de capabilities para que la mayoría de aplicaciones
funcionen:

### Capabilities que Docker MANTIENE por defecto (14)

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
| `SYS_ADMIN` | mount, pivot_root, BPF, namespaces, sethostname | **La más peligrosa** — permite escapar |
| `NET_ADMIN` | Configurar interfaces, firewall, routing | Manipulación de la red del host |
| `SYS_PTRACE` | Trazar procesos (ptrace, strace) | Inspeccionar otros contenedores |
| `SYS_MODULE` | Cargar módulos del kernel | Acceso directo al kernel |
| `SYS_RAWIO` | Acceso a puertos I/O (ioperm, iopl) | Acceso al hardware |
| `SYS_TIME` | Cambiar el reloj del sistema | Afecta a todo el host |
| `SYS_BOOT` | Rebootear el sistema | Afecta al host |
| `MAC_ADMIN` | Configurar SELinux/AppArmor | Debilitar seguridad del host |
| `SYSLOG` | Operar con el kernel syslog | Información sensible |
| `SYS_RESOURCE` | Sobrescribir resource limits | Puede consumir recursos ilimitados |

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

# Añadir NET_ADMIN: permite configurar interfaces de red
docker run --rm --cap-add NET_ADMIN alpine ip link add dummy0 type dummy 2>&1
# Puede añadir interfaces de red
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
# CapPrm: 00000000a80425fb  ← capabilities permitidas
# CapEff: 00000000a80425fb  ← capabilities efectivas
# CapBnd: 00000000a80425fb  ← bounding set (máximo permitido)
# CapAmb: 0000000000000000  ← ambient (heredadas en execve)

# Decodificar el bitmask (en el host con capsh instalado)
capsh --decode=00000000a80425fb
# 0x00000000a80425fb=cap_chown,cap_dac_override,cap_fowner,...
```

Significado de cada campo:

| Campo | Significado |
|---|---|
| `CapInh` | Heredadas del proceso padre |
| `CapPrm` | Capabilities que el proceso puede activar |
| `CapEff` | Capabilities actualmente activas |
| `CapBnd` | Límite máximo — no se pueden añadir más allá de este |
| `CapAmb` | Heredadas automáticamente al ejecutar nuevos programas |

```bash
# Con --cap-drop ALL: todo a cero
docker run --rm --cap-drop ALL alpine sh -c 'cat /proc/1/status | grep CapEff'
# CapEff: 0000000000000000  — sin capabilities

# Con --cap-drop ALL + cap-add NET_BIND_SERVICE: solo un bit activo
docker run --rm --cap-drop ALL --cap-add NET_BIND_SERVICE alpine \
  sh -c 'cat /proc/1/status | grep CapEff'
# CapEff: 0000000000000400  — solo NET_BIND_SERVICE (bit 10)
```

## seccomp — Filtrado de syscalls

seccomp (Secure Computing Mode) opera a un nivel diferente que las capabilities.
Mientras las capabilities controlan **qué privilegios** tiene un proceso, seccomp
controla **qué syscalls** puede ejecutar.

```
Capabilities: ¿tiene el proceso PERMISO para esta operación?
seccomp:      ¿puede el proceso EJECUTAR esta syscall?

Ejemplo con mount():
  1. seccomp verifica si mount() está permitida → si no, EPERM
  2. Si pasa seccomp, el kernel verifica CAP_SYS_ADMIN → si no, EPERM
  3. Solo si pasa AMBAS verificaciones, mount() se ejecuta
```

Docker aplica un perfil seccomp por defecto que bloquea ~44 syscalls peligrosas.

### Syscalls bloqueadas por defecto

| Syscall | Qué hace | Riesgo |
|---|---|---|
| `mount` / `umount2` | Montar/desmontar filesystems | Escapar del contenedor |
| `reboot` | Reiniciar el sistema | Afecta al host |
| `kexec_load` | Cargar un nuevo kernel | Reemplazar el kernel |
| `init_module` / `finit_module` | Cargar módulo del kernel | Código en kernel space |
| `pivot_root` | Cambiar el root filesystem | Escapar del contenedor |
| `swapon` / `swapoff` | Gestionar swap | Afecta rendimiento del host |
| `clock_settime` / `settimeofday` | Cambiar el reloj | Afecta a todo el host |
| `unshare` | Crear nuevos namespaces | Escapar del aislamiento |
| `acct` | Activar process accounting | Información sensible |

### Verificar seccomp

```bash
# Verificar que seccomp está activo
docker info | grep -i seccomp
# seccomp
#  Profile: builtin

# Un contenedor por defecto no puede usar mount (bloqueado por seccomp Y capabilities)
docker run --rm alpine mount -t tmpfs none /tmp 2>&1
# mount: permission denied
```

### Deshabilitar seccomp (no recomendado)

```bash
# seccomp=unconfined permite TODAS las syscalls
# Pero mount TAMBIÉN requiere CAP_SYS_ADMIN (capability)
# Solo seccomp=unconfined NO es suficiente para mount:
docker run --rm --security-opt seccomp=unconfined alpine mount -t tmpfs none /mnt 2>&1
# mount: permission denied — aún falta CAP_SYS_ADMIN

# Se necesitan AMBOS: seccomp desactivado + capability añadida
docker run --rm \
  --security-opt seccomp=unconfined \
  --cap-add SYS_ADMIN \
  alpine sh -c 'mount -t tmpfs none /mnt && df -h /mnt'
# tmpfs  ...  /mnt  — ahora sí funciona
```

### Perfiles seccomp custom

Se puede crear un perfil JSON que permita o bloquee syscalls específicas:

```json
{
  "defaultAction": "SCMP_ACT_ALLOW",
  "architectures": ["SCMP_ARCH_X86_64"],
  "syscalls": [
    {
      "names": ["mount", "umount2", "reboot", "kexec_load"],
      "action": "SCMP_ACT_ERRNO"
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

## AppArmor y SELinux

Además de capabilities y seccomp, Docker puede usar MAC (Mandatory Access Control):

**AppArmor** (Ubuntu/Debian):

```bash
# Ver el perfil AppArmor de un contenedor
docker inspect --format '{{.AppArmorProfile}}' container_name
# docker-default

# Ejecutar sin AppArmor (no recomendado)
docker run --rm --security-opt apparmor=unconfined alpine sh
```

**SELinux** (RHEL/Fedora):

```bash
# Desactivar SELinux para el contenedor
docker run --rm --security-opt label=disable alpine sh

# Niveles de MCS (Multi-Category Security)
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
```

```bash
# Con --privileged, el contenedor puede hacer casi todo lo que root puede en el host
docker run --rm --privileged alpine sh -c \
  'mount -t tmpfs none /mnt && ls /dev | wc -l'
# mount funciona, acceso a todos los devices (~200+)

# Puede ver los discos del host
docker run --rm --privileged alpine fdisk -l 2>/dev/null | head -5
```

### Cuándo es legítimo usar `--privileged`

| Caso | Por qué |
|---|---|
| Docker-in-Docker (dind) | Necesita montar overlayfs, crear namespaces |
| Acceso a hardware (GPU, USB) | Necesita `/dev/*` específico |
| Herramientas de debugging del kernel | strace, perf, bpf |
| Builds de imágenes con Buildkit | Algunas operaciones de build requieren mount |

En **todos los demás casos**, usar `--cap-add` con las capabilities específicas
es preferible a `--privileged`.

### Alternativa a `--privileged`: `--device`

```bash
# Solo exponer dispositivos específicos, no todos
docker run --rm --device=/dev/fuse alpine ls /dev/fuse
# Solo /dev/fuse está disponible

# Comparar cantidad de devices
docker run --rm alpine ls /dev | wc -l
# ~15 devices

docker run --rm --privileged alpine ls /dev | wc -l
# ~200+ devices
```

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

---

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

# Eliminar SETUID → su falla
docker run --rm --cap-drop SETUID alpine su -s /bin/sh nobody -c 'id' 2>&1
# su: can't set uid

# Ver las capabilities activas
docker run --rm alpine cat /proc/1/status | grep Cap
```

**Predicción**: cada `--cap-drop` elimina una capacidad específica. Sin `NET_RAW`,
`ping` no puede crear raw sockets. Sin `CHOWN`, `chown` falla. Sin `SETUID`, `su`
no puede cambiar de usuario.

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
# OK — ambas operaciones funcionan
```

**Predicción**: `--cap-drop ALL` deja CapEff en 0. Cada `--cap-add` restaura
solo la capability especificada.

### Ejercicio 3 — Comparar normal vs privileged

```bash
# Normal: mount no funciona (bloqueado por seccomp + falta CAP_SYS_ADMIN)
docker run --rm alpine mount -t tmpfs none /mnt 2>&1
# mount: permission denied

# Privileged: mount funciona (seccomp desactivado + todas las capabilities)
docker run --rm --privileged alpine sh -c \
  'mount -t tmpfs none /mnt && echo "mount OK" && ls /dev | wc -l'
# mount OK
# ~200+ devices visibles

# Normal: devices limitados
docker run --rm alpine ls /dev | wc -l
# ~15 devices

# Comparar capabilities
echo "Normal:"
docker run --rm alpine cat /proc/1/status | grep CapEff
# 00000000a80425fb — 14 capabilities

echo "Privileged:"
docker run --rm --privileged alpine cat /proc/1/status | grep CapEff
# 000001ffffffffff — TODAS las capabilities
```

**Predicción**: `--privileged` cambia el bitmask de `a80425fb` (14 caps) a
`1ffffffffff` (todas). Mount funciona porque tiene TANTO la capability SYS_ADMIN
como seccomp desactivado.

### Ejercicio 4 — Decodificar capabilities

```bash
# Ver el bitmask de capabilities
docker run --rm alpine cat /proc/1/status | grep Cap

# Decodificar en el host (necesitas capsh del paquete libcap)
capsh --decode=00000000a80425fb

# Comparar con --cap-drop ALL
docker run --rm --cap-drop ALL alpine cat /proc/1/status | grep CapEff
# CapEff: 0000000000000000  — vacío

# Comparar con --cap-drop ALL + una sola capability
docker run --rm --cap-drop ALL --cap-add NET_BIND_SERVICE alpine \
  cat /proc/1/status | grep CapEff
# CapEff: 0000000000000400  — solo bit 10 (NET_BIND_SERVICE)

# Comparar con --privileged
docker run --rm --privileged alpine cat /proc/1/status | grep CapEff
# CapEff: 000001ffffffffff  — todos los bits activos
```

**Predicción**: cada capability es un bit en el bitmask. `0x400` = bit 10 =
NET_BIND_SERVICE. El bitmask permite ver exactamente qué capabilities tiene
un proceso.

### Ejercicio 5 — seccomp: doble barrera con capabilities

```bash
# mount está bloqueado por DOS mecanismos: seccomp Y capabilities

# 1. Desactivar seccomp pero SIN CAP_SYS_ADMIN: sigue fallando
docker run --rm --security-opt seccomp=unconfined alpine mount -t tmpfs none /mnt 2>&1
# mount: permission denied — falta CAP_SYS_ADMIN

# 2. Añadir CAP_SYS_ADMIN pero CON seccomp default: sigue fallando
docker run --rm --cap-add SYS_ADMIN alpine mount -t tmpfs none /mnt 2>&1
# mount: permission denied — seccomp bloquea la syscall

# 3. AMBOS desactivados: funciona
docker run --rm --security-opt seccomp=unconfined --cap-add SYS_ADMIN alpine \
  sh -c 'mount -t tmpfs none /mnt && echo "mount OK"'
# mount OK

# Esto demuestra la defensa en profundidad:
# hay que superar TODAS las capas para ejecutar una operación
```

**Predicción**: `mount` necesita pasar seccomp (syscall permitida) Y capabilities
(CAP_SYS_ADMIN). Desactivar solo una capa no es suficiente.

### Ejercicio 6 — Perfil seccomp custom

```bash
# Crear un perfil que permite todo EXCEPTO mount y reboot
cat > /tmp/custom-seccomp.json << 'EOF'
{
  "defaultAction": "SCMP_ACT_ALLOW",
  "architectures": ["SCMP_ARCH_X86_64"],
  "syscalls": [
    {
      "names": ["mount", "umount2", "reboot"],
      "action": "SCMP_ACT_ERRNO"
    }
  ]
}
EOF

# ls funciona (permitido por default)
docker run --rm --security-opt seccomp=/tmp/custom-seccomp.json alpine ls / > /dev/null && echo "ls OK"
# ls OK

# mount bloqueado por el perfil custom (además de por capabilities)
docker run --rm --security-opt seccomp=/tmp/custom-seccomp.json --cap-add SYS_ADMIN alpine \
  mount -t tmpfs none /mnt 2>&1
# mount: permission denied — bloqueado por nuestro perfil seccomp

rm /tmp/custom-seccomp.json
```

**Predicción**: el perfil custom con `defaultAction: ALLOW` permite todo excepto
las syscalls explícitamente bloqueadas. Mount falla aunque tenga CAP_SYS_ADMIN
porque seccomp lo bloquea.

### Ejercicio 7 — --device vs --privileged

```bash
# Sin nada: pocos devices
docker run --rm alpine ls /dev | wc -l
# ~15 devices

# Con --device: solo el device específico se añade
docker run --rm --device=/dev/null alpine ls -la /dev/null
# crw-rw-rw- 1 root root 1, 3 ...

# Con --privileged: TODOS los devices
docker run --rm --privileged alpine ls /dev | wc -l
# ~200+ devices

# --device es más seguro que --privileged para acceso a hardware
```

**Predicción**: `--device` expone solo el dispositivo solicitado. `--privileged`
expone todos los devices del host además de desactivar todas las restricciones.

### Ejercicio 8 — Combinar múltiples capas de seguridad

```bash
# Aplicar la configuración más segura posible
docker run -d \
  --name secure-container \
  --user 1000:1000 \
  --cap-drop ALL \
  --security-opt no-new-privileges:true \
  --read-only \
  --tmpfs /tmp:size=64m,mode=1777 \
  alpine sleep 300

# Verificar las restricciones
docker exec secure-container id
# uid=1000 gid=1000

docker exec secure-container cat /proc/1/status | grep CapEff
# CapEff: 0000000000000000  — sin capabilities

docker exec secure-container cat /proc/1/status | grep NoNewPrivs
# NoNewPrivs: 1

docker exec secure-container sh -c 'echo "test" > /test.txt' 2>&1
# Read-only file system

docker exec secure-container sh -c 'echo "test" > /tmp/test.txt && echo "tmpfs OK"'
# tmpfs OK — /tmp es writable (tmpfs)

# Limpiar
docker rm -f secure-container
```

**Predicción**: el contenedor corre como non-root, sin capabilities, sin
escalación de privilegios, con filesystem read-only. Solo `/tmp` (tmpfs) y
volúmenes son writables.
