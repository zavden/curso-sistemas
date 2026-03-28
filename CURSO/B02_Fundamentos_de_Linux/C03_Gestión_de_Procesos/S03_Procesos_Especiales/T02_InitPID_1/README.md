# T02 — Init / PID 1

## Qué es PID 1

PID 1 es el **primer proceso de espacio de usuario** que el kernel lanza
después de completar su inicialización. Todos los demás procesos del sistema
descienden de él, directa o indirectamente:

```
Secuencia de arranque:
  BIOS/UEFI → bootloader (GRUB) → kernel → PID 1

El kernel:
  1. Monta el rootfs (o initramfs)
  2. Ejecuta /sbin/init (o lo que indique init= en la línea del kernel)
  3. /sbin/init se convierte en PID 1
  4. PID 1 inicia todo lo demás
```

En distribuciones modernas (Debian 8+, RHEL 7+, Ubuntu 15.04+), PID 1 es
**systemd**:

```bash
ls -la /sbin/init
# lrwxrwxrwx 1 root root ... /sbin/init -> /lib/systemd/systemd

ps -p 1 -o pid,cmd
# PID CMD
#   1 /usr/lib/systemd/systemd --switched-root --system --deserialize ...
```

## Responsabilidades de PID 1

### 1. Inicialización del sistema

PID 1 arranca todos los servicios y configura el sistema:

```
systemd como PID 1:
  1. Monta filesystems (/proc, /sys, /dev, los de /etc/fstab)
  2. Configura el hostname
  3. Activa swap
  4. Inicia servicios en paralelo según dependencias
  5. Levanta la red
  6. Lanza las sesiones de login (getty, sshd, display manager)
```

### 2. Reaping de procesos huérfanos

Esta es la responsabilidad más relevante para entender la gestión de procesos.
Cuando un proceso padre muere, sus hijos son re-parenteados a PID 1. PID 1
**debe** hacer `wait()` por estos huérfanos para evitar que se acumulen como
zombies:

```
Proceso padre (PID 500) muere
  └── hijo huérfano (PID 501)
        ↓
      re-parenteado a PID 1
        ↓
      cuando PID 501 termina, PID 1 hace wait() automáticamente
        ↓
      PID 501 se limpia correctamente (no queda zombie)
```

SysVinit, Upstart y systemd implementan este reaping. Un programa arbitrario
como PID 1 (en contenedores) **no lo hace automáticamente**.

### 3. Gestión de señales

PID 1 tiene un tratamiento especial de señales en el kernel:

```bash
# Protección de PID 1:
# El kernel NO entrega señales a PID 1 si PID 1 no tiene un handler
# explícito para esa señal. Esto incluye SIGTERM y SIGINT.
#
# Solo SIGKILL puede matar PID 1, y solo en ciertos contextos.
#
# ¿Por qué? Si PID 1 muere, el sistema entra en kernel panic.
# Esta protección evita que un kill accidental tumbe el sistema.

# En el host:
kill -TERM 1     # No hace nada (systemd no muere)
kill -KILL 1     # Kernel panic (nunca hacer esto)

# En contenedores:
# SIGKILL a PID 1 del contenedor mata el contenedor
# SIGTERM a PID 1 puede ser ignorada si PID 1 no tiene handler
```

### 4. Transiciones de estado del sistema

PID 1 gestiona los cambios de estado: arranque, apagado, reinicio, cambio de
runlevel/target:

```bash
# systemd como PID 1 gestiona:
systemctl poweroff     # PID 1 coordina el apagado ordenado
systemctl reboot       # PID 1 detiene servicios y reinicia
systemctl isolate multi-user.target   # cambio de target

# El apagado ordenado:
# 1. systemd envía SIGTERM a todos los servicios
# 2. Espera TimeoutStopSec (default 90s)
# 3. Envía SIGKILL a los que no terminaron
# 4. Desmonta filesystems
# 5. Envía señal al kernel para apagar/reiniciar
```

## Historia de init systems

### SysVinit (1983–2010s)

El init original de System V Unix:

```bash
# Configuración: /etc/inittab
# Runlevels: 0 (halt), 1 (single), 2-5 (multi-user), 6 (reboot)
# Scripts en /etc/init.d/ ejecutados secuencialmente

# Arranque secuencial — lento:
# /etc/init.d/networking start   (espera...)
# /etc/init.d/sshd start         (espera...)
# /etc/init.d/apache2 start      (espera...)

# Cada servicio se iniciaba uno tras otro
# Un servicio lento bloqueaba a todos los siguientes
```

### Upstart (2006–2015)

Usado por Ubuntu durante algunos años:

```bash
# Event-driven: servicios se inician al ocurrir eventos
# start on filesystem and net-device-up IFACE=lo
# Mejor paralelismo que SysVinit
# Reemplazado por systemd en Ubuntu 15.04
```

### systemd (2010–presente)

El estándar actual en prácticamente todas las distribuciones mayores:

```bash
# Arranque paralelo con grafo de dependencias
# Socket activation: el servicio se inicia al recibir la primera conexión
# Cgroups: cada servicio en su propio cgroup (aislamiento de recursos)
# Journal: logging centralizado y estructurado
# Unit files: declarativos, no scripts imperativos

# Verificar que systemd es PID 1
systemctl --version
# systemd 252 (252.22-1~deb12u1)
```

## systemd como PID 1 — Funcionalidades clave

### Arranque paralelo

```
SysVinit (secuencial):
  [networking]──[sshd]──[nginx]──[postgres]──[app]
  Total: suma de tiempos individuales

systemd (paralelo):
  [networking]─┐
  [sshd]───────┤
  [nginx]──────┼── [app] (espera solo a sus dependencias)
  [postgres]───┘
  Total: el camino más largo del grafo
```

```bash
# Ver cuánto tardó el arranque
systemd-analyze
# Startup finished in 2.5s (kernel) + 4.1s (userspace) = 6.6s

# Ver qué servicios tardaron más
systemd-analyze blame
# 3.2s  NetworkManager-wait-online.service
# 1.1s  docker.service
# 0.8s  postgresql.service

# Diagrama SVG del arranque
systemd-analyze plot > boot.svg
```

### Socket activation

```bash
# systemd puede escuchar en un socket ANTES de iniciar el servicio
# Al recibir la primera conexión, inicia el servicio y le pasa el socket
#
# Ventajas:
# - El servicio se inicia bajo demanda (ahorra recursos)
# - El socket acepta conexiones durante el arranque
#   (las encola hasta que el servicio está listo)
# - Permite arranque paralelo sin importar orden de dependencias
```

### Cgroups por servicio

```bash
# Cada servicio de systemd corre en su propio cgroup
systemd-cgls
# Control group /:
# -.slice
# ├─init.scope
# │ └─1 /usr/lib/systemd/systemd
# ├─system.slice
# │ ├─sshd.service
# │ │ └─842 /usr/sbin/sshd -D
# │ ├─nginx.service
# │ │ ├─1200 nginx: master process
# │ │ └─1201 nginx: worker process

# Esto permite:
# - Matar un servicio y TODOS sus procesos hijos (sin dejar huérfanos)
# - Limitar recursos (CPU, memoria) por servicio
# - Contabilizar uso de recursos por servicio
```

## PID 1 en contenedores

Dentro de un contenedor, el entrypoint se convierte en PID 1 del PID namespace
del contenedor. Esto causa problemas si el proceso no está diseñado para ser
PID 1:

### El problema

```bash
# Dockerfile típico:
# CMD ["python3", "app.py"]
#
# python3 es PID 1 dentro del contenedor
#
# Problemas:
# 1. python3 no hace wait() por hijos → zombies se acumulan
# 2. python3 no tiene handler para SIGTERM → docker stop tarda 10s
#    (espera timeout y luego SIGKILL)
# 3. Si python3 lanza subprocesos que mueren, quedan como zombies
```

### Solución: tini / dumb-init

```bash
# tini es un PID 1 mínimo diseñado para contenedores
# Hace exactamente dos cosas:
# 1. Reenvía señales al proceso hijo
# 2. Hace wait() para recoger zombies

# Docker --init (usa tini internamente)
docker run --init myimage

# Explícitamente en Dockerfile
# FROM debian:12
# RUN apt-get update && apt-get install -y tini
# ENTRYPOINT ["/usr/bin/tini", "--"]
# CMD ["python3", "app.py"]

# Alternativa: dumb-init (de Yelp)
# ENTRYPOINT ["/usr/bin/dumb-init", "--"]
# CMD ["python3", "app.py"]
```

### El problema del shell como entrypoint

```bash
# MAL: shell form (crea un shell como PID 1)
# CMD python3 app.py
#
# Docker lo ejecuta como: /bin/sh -c "python3 app.py"
# PID 1 = /bin/sh, PID 2 = python3
# El shell no reenvía señales → python3 nunca recibe SIGTERM

# BIEN: exec form (python3 es PID 1 directamente)
# CMD ["python3", "app.py"]

# MEJOR: exec form con tini
# ENTRYPOINT ["/usr/bin/tini", "--"]
# CMD ["python3", "app.py"]
```

## PID 1 y kernel threads

```bash
# Los kernel threads (entre corchetes en ps) son hijos de kthreadd (PID 2),
# no de PID 1
ps -eo pid,ppid,cmd | head -5
# PID PPID CMD
#   1    0 /usr/lib/systemd/systemd
#   2    0 [kthreadd]
#   3    2 [rcu_gp]           ← hijo de kthreadd
#   4    2 [rcu_par_gp]       ← hijo de kthreadd

# PID 1 = primer proceso de usuario
# PID 2 = padre de todos los kernel threads
# Ambos tienen PPID 0 (el kernel)
```

---

## Ejercicios

### Ejercicio 1 — Verificar PID 1

```bash
# ¿Qué es PID 1 en tu sistema?
ps -p 1 -o pid,cmd

# ¿Es systemd?
ls -la /sbin/init

# ¿Cuánto tardó en arrancar?
systemd-analyze

# ¿Qué servicios tardaron más?
systemd-analyze blame | head -5
```

### Ejercicio 2 — Observar reaping

```bash
# Crear un huérfano y ver cómo PID 1 lo adopta
bash -c 'sleep 60 & echo "hijo=$!"; exit' &
sleep 1

# ¿Quién es el padre?
ps -o pid,ppid,cmd -p $(pgrep -nf 'sleep 60')
# PPID debe ser 1

# Cuando el sleep termine, PID 1 hará wait()
# (no quedará zombie)
pkill -f 'sleep 60'
```

### Ejercicio 3 — Cgroups por servicio

```bash
# Ver la jerarquía de cgroups de systemd
systemd-cgls --no-pager | head -30

# ¿En qué cgroup está tu shell?
cat /proc/$$/cgroup

# ¿En qué cgroup está sshd?
SSHD_PID=$(pgrep -x sshd | head -1)
cat /proc/$SSHD_PID/cgroup 2>/dev/null || echo "sshd no está corriendo"
```
