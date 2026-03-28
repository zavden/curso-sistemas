# T02 — cgroups v2

## Qué son los cgroups

**cgroups** (control groups) son una funcionalidad del kernel que permite
**limitar, contabilizar y aislar** el uso de recursos (CPU, memoria, I/O,
número de procesos) de un grupo de procesos:

```
Namespaces:  "qué puede VER un proceso"  (aislamiento de visibilidad)
cgroups:     "cuánto puede USAR un proceso"  (limitación de recursos)

Un contenedor usa ambos:
  - Namespaces para que el proceso crea que tiene su propio sistema
  - cgroups para que no consuma todos los recursos del host
```

Sin cgroups, un contenedor podría consumir toda la CPU, toda la memoria y
todo el I/O del host, afectando a otros contenedores y al propio sistema.

## cgroups v1 vs v2

```
cgroups v1 (2008):
  - Cada controlador (cpu, memory, io) tiene su propia jerarquía
  - Un proceso puede estar en cgroups diferentes para cpu y memory
  - Complejo, inconsistente, difícil de gestionar

cgroups v2 (2016, estable desde kernel 4.15):
  - Una sola jerarquía unificada para todos los controladores
  - Un proceso está en UN solo cgroup
  - API consistente y más simple
  - Mejor contabilidad de recursos
```

| Aspecto | v1 | v2 |
|---|---|---|
| Jerarquía | Múltiples (una por controlador) | Una sola unificada |
| Mount point | `/sys/fs/cgroup/cpu/`, `/sys/fs/cgroup/memory/`, ... | `/sys/fs/cgroup/` (todo junto) |
| Complejidad | Alta | Menor |
| Distribuciones | Legacy | Default en Debian 11+, RHEL 9+, Ubuntu 21.10+ |

```bash
# Verificar qué versión está activa
mount | grep cgroup
# cgroup2 on /sys/fs/cgroup type cgroup2 (...)
#                                 ^^^^^^^
#                                 cgroup2 = v2

# Si ves "cgroup" (sin 2) y múltiples mount points → v1
# Si ves "cgroup2" y un solo mount point → v2

# Algunas distribuciones montan ambos (modo híbrido)
# RHEL 8: v1 por defecto, v2 opcional
# RHEL 9, Debian 11+, Ubuntu 22.04+: v2 por defecto

stat -fc %T /sys/fs/cgroup/
# cgroup2fs → v2
# tmpfs → v1 (la raíz es tmpfs con subdirectorios por controlador)
```

Este tema cubre **solo v2** por ser el estándar actual.

## Jerarquía de cgroups v2

```bash
ls /sys/fs/cgroup/
# cgroup.controllers      ← controladores disponibles
# cgroup.procs            ← PIDs en este cgroup (raíz)
# cgroup.subtree_control  ← controladores habilitados para hijos
# cpu.stat                ← estadísticas de CPU
# memory.current          ← uso de memoria actual
# io.stat                 ← estadísticas de I/O
# pids.current            ← número de procesos
# system.slice/           ← cgroup de servicios systemd
# user.slice/             ← cgroup de sesiones de usuario
# init.scope/             ← cgroup de PID 1
```

La jerarquía es un **árbol de directorios**. Cada directorio es un cgroup.
Crear un cgroup es simplemente crear un directorio:

```
/sys/fs/cgroup/              ← cgroup raíz
├── system.slice/            ← servicios del sistema
│   ├── sshd.service/        ← cgroup de sshd
│   ├── nginx.service/       ← cgroup de nginx
│   └── docker.service/
├── user.slice/              ← sesiones de usuario
│   └── user-1000.slice/
│       └── session-1.scope/
└── init.scope/              ← PID 1
```

## Controladores

Los controladores son los módulos que limitan recursos específicos:

```bash
# Ver controladores disponibles
cat /sys/fs/cgroup/cgroup.controllers
# cpuset cpu io memory hugetlb pids rdma misc

# Ver controladores habilitados para sub-cgroups
cat /sys/fs/cgroup/cgroup.subtree_control
# cpu io memory pids
```

### Controlador memory

```bash
# Uso actual de memoria del cgroup
cat /sys/fs/cgroup/system.slice/sshd.service/memory.current
# 5242880  (en bytes = 5MB)

# Límite de memoria (si está establecido)
cat /sys/fs/cgroup/system.slice/sshd.service/memory.max
# max  (sin límite)

# Estadísticas detalladas
cat /sys/fs/cgroup/system.slice/sshd.service/memory.stat
# anon 1048576        ← memoria anónima (heap, stack)
# file 3145728        ← page cache (archivos mapeados)
# shmem 0             ← shared memory
# ...
```

### Controlador cpu

```bash
# Limitar CPU: peso relativo (no porcentaje absoluto)
cat /sys/fs/cgroup/system.slice/sshd.service/cpu.weight
# 100  (default, rango 1-10000)

# Limitar CPU: máximo absoluto
cat /sys/fs/cgroup/system.slice/sshd.service/cpu.max
# max 100000
# ^   ^
# |   periodo en microsegundos (100ms)
# cuota: "max" = sin límite, o un número de microsegundos

# cpu.max = "50000 100000" → 50ms de cada 100ms → 50% de un core
# cpu.max = "200000 100000" → 200ms de cada 100ms → 2 cores máximo
```

### Controlador io

```bash
# Limitar I/O de disco
cat /sys/fs/cgroup/system.slice/nginx.service/io.max
# 8:0 rbps=max wbps=max riops=max wiops=max
# ^^^
# major:minor del dispositivo

# Establecer límite: máximo 10MB/s de lectura en sda (8:0)
# echo "8:0 rbps=10485760" > io.max
```

### Controlador pids

```bash
# Límite de número de procesos/threads en el cgroup
cat /sys/fs/cgroup/system.slice/nginx.service/pids.max
# max  (sin límite)

cat /sys/fs/cgroup/system.slice/nginx.service/pids.current
# 5

# pids.max previene fork bombs dentro de un contenedor
# docker run --pids-limit 100 → establece pids.max = 100
```

## systemd y cgroups

systemd crea automáticamente un cgroup por cada servicio, scope y slice:

```bash
# Ver la jerarquía de cgroups de systemd
systemd-cgls
# Control group /:
# -.slice
# ├─init.scope
# │ └─1 /usr/lib/systemd/systemd
# ├─system.slice
# │ ├─sshd.service
# │ │ └─842 /usr/sbin/sshd
# │ ├─nginx.service
# │ │ ├─1200 nginx: master
# │ │ └─1201 nginx: worker
# │ └─docker.service
# │   └─900 /usr/bin/dockerd
# └─user.slice
#   └─user-1000.slice
#     └─session-1.scope
#       ├─1500 sshd: dev
#       └─1506 -bash

# Slices: agrupaciones jerárquicas
# system.slice  → servicios del sistema
# user.slice    → sesiones de usuario
# machine.slice → VMs y contenedores (si existe)

# Ver a qué cgroup pertenece un proceso
cat /proc/$$/cgroup
# 0::/user.slice/user-1000.slice/session-1.scope
```

### Limitar recursos de un servicio con systemd

```bash
# Limitar memoria de un servicio
sudo systemctl set-property nginx.service MemoryMax=512M

# Limitar CPU
sudo systemctl set-property nginx.service CPUQuota=50%

# Limitar número de procesos
sudo systemctl set-property nginx.service TasksMax=100

# Estas propiedades se traducen a archivos en el cgroup:
# MemoryMax=512M → memory.max = 536870912
# CPUQuota=50%   → cpu.max = "50000 100000"
# TasksMax=100   → pids.max = 100

# Ver los límites actuales de un servicio
systemctl show nginx.service | grep -E 'Memory|CPU|Tasks'

# En el unit file directamente:
# [Service]
# MemoryMax=512M
# CPUQuota=50%
# TasksMax=100
```

## Crear un cgroup manualmente

```bash
# Crear un cgroup
sudo mkdir /sys/fs/cgroup/test-group

# Habilitar controladores para el cgroup
# Primero verificar qué controladores están disponibles en el padre
cat /sys/fs/cgroup/cgroup.subtree_control
# cpu io memory pids

# Establecer un límite de memoria (100MB)
echo 104857600 | sudo tee /sys/fs/cgroup/test-group/memory.max

# Establecer un límite de PIDs
echo 20 | sudo tee /sys/fs/cgroup/test-group/pids.max

# Mover un proceso al cgroup
echo $$ | sudo tee /sys/fs/cgroup/test-group/cgroup.procs

# Verificar
cat /proc/$$/cgroup
# 0::/test-group

# Ver uso actual
cat /sys/fs/cgroup/test-group/memory.current
cat /sys/fs/cgroup/test-group/pids.current

# Limpiar: mover el proceso de vuelta y eliminar el cgroup
echo $$ | sudo tee /sys/fs/cgroup/cgroup.procs
sudo rmdir /sys/fs/cgroup/test-group
```

### Qué pasa cuando se excede un límite

```bash
# Memoria: si un proceso excede memory.max
# → El kernel intenta recuperar memoria del cgroup (reclaim)
# → Si no puede, invoca el OOM killer dentro del cgroup
# → El proceso (o uno de sus hijos) es matado con SIGKILL
# → dmesg muestra: "Memory cgroup out of memory: Killed process..."

# Se puede ver el conteo de OOM kills:
cat /sys/fs/cgroup/test-group/memory.events
# low 0
# high 0
# max 0
# oom 3        ← 3 procesos matados por OOM
# oom_kill 3

# PIDs: si se intenta crear más procesos que pids.max
# → fork() retorna EAGAIN (error)
# → No se puede crear ningún proceso nuevo hasta que alguno termine
```

## Ver cgroup de un proceso

```bash
# Cgroup de un proceso específico
cat /proc/1234/cgroup
# 0::/system.slice/nginx.service

# Cgroup de tu shell
cat /proc/$$/cgroup

# Cgroup de un contenedor Docker (desde el host)
CPID=$(docker inspect --format '{{.State.Pid}}' mi-container)
cat /proc/$CPID/cgroup
# 0::/system.slice/docker-<hash>.scope

# Desde DENTRO del contenedor (con cgroup namespace):
cat /proc/self/cgroup
# 0::/
# (ve su cgroup como raíz gracias al cgroup namespace)
```

## cgroups y Docker

```bash
# Docker usa cgroups para implementar --memory, --cpus, --pids-limit:
docker run --memory 256m --cpus 0.5 --pids-limit 50 alpine sleep 300

# Verificar desde el host
CPID=$(docker inspect --format '{{.State.Pid}}' <container>)
CGROUP=$(cat /proc/$CPID/cgroup | cut -d: -f3)

cat /sys/fs/cgroup$CGROUP/memory.max    # 268435456 (256MB)
cat /sys/fs/cgroup$CGROUP/cpu.max       # 50000 100000 (50%)
cat /sys/fs/cgroup$CGROUP/pids.max      # 50
```

---

## Ejercicios

### Ejercicio 1 — Explorar la jerarquía

```bash
# ¿Qué versión de cgroups usa tu sistema?
mount | grep cgroup

# Ver la jerarquía completa
systemd-cgls --no-pager | head -30

# ¿En qué cgroup estás tú?
cat /proc/$$/cgroup

# ¿Qué controladores están disponibles?
cat /sys/fs/cgroup/cgroup.controllers
```

### Ejercicio 2 — Ver recursos de un servicio

```bash
# Elegir un servicio corriendo
SERVICE=sshd  # o ssh, cron, etc.
CGROUP_PATH="/sys/fs/cgroup/system.slice/${SERVICE}.service"

if [ -d "$CGROUP_PATH" ]; then
    echo "Memoria usada: $(cat $CGROUP_PATH/memory.current) bytes"
    echo "Límite memoria: $(cat $CGROUP_PATH/memory.max)"
    echo "PIDs actuales: $(cat $CGROUP_PATH/pids.current)"
    echo "Límite PIDs: $(cat $CGROUP_PATH/pids.max)"
else
    echo "Cgroup no encontrado — probar con: ls /sys/fs/cgroup/system.slice/"
fi
```

### Ejercicio 3 — Crear un cgroup con límite de memoria

```bash
# Crear cgroup
sudo mkdir /sys/fs/cgroup/ejercicio

# Limitar a 50MB
echo 52428800 | sudo tee /sys/fs/cgroup/ejercicio/memory.max

# Mover tu shell al cgroup
echo $$ | sudo tee /sys/fs/cgroup/ejercicio/cgroup.procs

# Verificar
cat /proc/$$/cgroup
cat /sys/fs/cgroup/ejercicio/memory.current

# Volver al cgroup original
echo $$ | sudo tee /sys/fs/cgroup/user.slice/cgroup.procs 2>/dev/null \
  || echo $$ | sudo tee /sys/fs/cgroup/cgroup.procs
sudo rmdir /sys/fs/cgroup/ejercicio
```
