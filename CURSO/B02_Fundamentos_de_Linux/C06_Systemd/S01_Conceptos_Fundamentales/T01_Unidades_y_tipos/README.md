# T01 — Unidades y tipos

## Qué es una unidad (unit)

En systemd, todo recurso gestionado se representa como una **unidad**. Un
servicio es una unidad, un punto de montaje es una unidad, un timer es una
unidad. El tipo de unidad se determina por la extensión del archivo:

```bash
# Ver todas las unidades cargadas:
systemctl list-units

# Ver todas las unidades instaladas (cargadas o no):
systemctl list-unit-files

# Filtrar por tipo:
systemctl list-units --type=service
systemctl list-units --type=timer
systemctl list-unit-files --type=socket
```

## Tipos de unidades

### service — Procesos y daemons

La unidad más común. Gestiona un proceso (daemon, servicio, tarea):

```bash
# Ejemplos:
sshd.service          # servidor SSH
nginx.service         # servidor web
docker.service        # Docker daemon
cron.service          # cron daemon (Debian)
crond.service         # cron daemon (RHEL)

systemctl status sshd.service
# El sufijo .service es el default — se puede omitir:
systemctl status sshd
```

```ini
# Estructura básica de un .service:
[Unit]
Description=My Application
After=network.target

[Service]
ExecStart=/usr/local/bin/myapp
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

### socket — Activación por socket

Systemd escucha en un socket (TCP, UDP, Unix) y arranca el servicio
correspondiente cuando llega una conexión. El servicio no consume recursos
hasta que se necesita:

```bash
# Ejemplos:
sshd.socket             # activa sshd.service al recibir conexión en :22
cups.socket             # activa CUPS al recibir conexión de impresión
docker.socket           # activa Docker al usar el socket /var/run/docker.sock

systemctl list-units --type=socket
```

```ini
# Ejemplo: sshd.socket
[Unit]
Description=OpenSSH Server Socket

[Socket]
ListenStream=22
Accept=yes

[Install]
WantedBy=sockets.target
```

```bash
# Ventaja de activación por socket:
# 1. El servicio no arranca hasta que alguien se conecta
# 2. Múltiples servicios pueden arrancar en paralelo
#    (cada socket está listo inmediatamente, sin esperar al servicio)
# 3. Si el servicio crashea, el socket sigue escuchando
#    y lo reactiva en la siguiente conexión

# Docker usa esto: docker.socket activa docker.service
# cuando un usuario ejecuta un comando docker
systemctl status docker.socket
systemctl status docker.service
```

### timer — Tareas programadas

Reemplazo moderno de cron. Cada timer tiene un service asociado que ejecuta:

```bash
# Ejemplos:
apt-daily.timer             # actualización diaria de paquetes (Debian)
apt-daily-upgrade.timer     # upgrade diario (Debian)
fstrim.timer                # TRIM semanal para SSDs
logrotate.timer             # rotación de logs
systemd-tmpfiles-clean.timer  # limpieza de /tmp

systemctl list-timers
# NEXT                        LEFT          LAST                        PASSED       UNIT
# Mon 2024-03-18 06:00:00 UTC  12h left      Sun 2024-03-17 06:00:00 UTC 5h ago       apt-daily.timer
```

```ini
# Ejemplo: backup.timer
[Unit]
Description=Daily backup

[Timer]
OnCalendar=daily
Persistent=true

[Install]
WantedBy=timers.target
```

### target — Grupos de unidades

Los targets agrupan unidades y definen estados del sistema. Son el reemplazo
de los runlevels de SysVinit:

```bash
# Targets principales:
poweroff.target         # apagar (runlevel 0)
rescue.target           # modo single-user (runlevel 1)
multi-user.target       # multiusuario sin GUI (runlevel 3)
graphical.target        # multiusuario con GUI (runlevel 5)
reboot.target           # reiniciar (runlevel 6)

# Otros targets importantes:
network.target          # red básica disponible
network-online.target   # red completamente configurada
local-fs.target         # sistemas de archivos locales montados
remote-fs.target        # sistemas de archivos remotos montados
sysinit.target          # inicialización del sistema
sockets.target          # todos los sockets listos
timers.target           # todos los timers listos
basic.target            # sistema básico funcional

systemctl get-default
# multi-user.target (o graphical.target)

systemctl list-dependencies multi-user.target
```

### mount y automount — Puntos de montaje

```bash
# Systemd puede gestionar puntos de montaje (generados desde /etc/fstab
# o como archivos .mount):

# El nombre de la unidad es la ruta con - en lugar de /:
# /home → home.mount
# /var/log → var-log.mount

systemctl list-units --type=mount
# home.mount
# tmp.mount
# boot-efi.mount
# ...

# automount — montar on-demand (al primer acceso):
# Similar a autofs
```

```ini
# Ejemplo: tmp.mount
[Unit]
Description=Temporary Directory

[Mount]
What=tmpfs
Where=/tmp
Type=tmpfs
Options=mode=1777,strictatime,nosuid,nodev

[Install]
WantedBy=local-fs.target
```

### slice — Control de recursos (cgroups)

Los slices organizan los procesos en una jerarquía de cgroups para controlar
recursos (CPU, memoria, I/O):

```bash
# Slices predefinidos:
-.slice                    # root slice
system.slice               # servicios del sistema
user.slice                 # sesiones de usuario
machine.slice              # contenedores y VMs

# Cada servicio pertenece a un slice:
systemctl status nginx.service
# ... CGroup: /system.slice/nginx.service

# Ver la jerarquía de slices:
systemd-cgls

# Cada usuario tiene su propio slice:
# user-1000.slice → sesión del UID 1000
```

```ini
# Ejemplo: limitar recursos de un slice
[Slice]
MemoryMax=2G
CPUQuota=50%
```

### scope — Procesos externos

Los scopes agrupan procesos que **no** fueron iniciados por systemd pero que
se quieren gestionar. Systemd los crea automáticamente para sesiones de
usuario y contenedores:

```bash
# Ejemplos:
session-1.scope           # sesión de login
init.scope                # PID 1

# No se crean con archivos .scope — se crean en runtime
systemctl list-units --type=scope
```

### path — Activación por cambio en el filesystem

Monitorea una ruta y activa un servicio cuando se detecta un cambio:

```bash
# Ejemplo: activar un servicio cuando aparece un archivo
```

```ini
# upload-watcher.path
[Unit]
Description=Watch for uploads

[Path]
PathExists=/var/spool/uploads/ready
MakeDirectory=true

[Install]
WantedBy=multi-user.target

# Cuando /var/spool/uploads/ready aparece,
# systemd arranca upload-watcher.service automáticamente
```

```ini
# Directivas de [Path]:
PathExists=/path         # activar cuando la ruta existe
PathExistsGlob=/path/*   # activar cuando el glob tiene resultados
PathChanged=/path        # activar cuando el archivo cambia
PathModified=/path       # activar cuando el archivo se modifica (incluye metadata)
DirectoryNotEmpty=/path  # activar cuando el directorio no está vacío
```

### swap — Dispositivos de swap

```bash
# Generados automáticamente desde /etc/fstab o como archivos .swap:
systemctl list-units --type=swap
# dev-sda2.swap

# Nombre basado en la ruta del dispositivo:
# /dev/sda2 → dev-sda2.swap
```

## Resumen de tipos

| Tipo | Extensión | Función | Ejemplo |
|---|---|---|---|
| service | `.service` | Procesos/daemons | `nginx.service` |
| socket | `.socket` | Activación por conexión | `sshd.socket` |
| timer | `.timer` | Tareas programadas | `apt-daily.timer` |
| target | `.target` | Agrupación de unidades | `multi-user.target` |
| mount | `.mount` | Puntos de montaje | `home.mount` |
| automount | `.automount` | Montaje on-demand | `home.automount` |
| slice | `.slice` | Control de recursos | `system.slice` |
| scope | `.scope` | Procesos externos | `session-1.scope` |
| path | `.path` | Activación por filesystem | `upload-watcher.path` |
| swap | `.swap` | Dispositivos de swap | `dev-sda2.swap` |

## Relaciones entre unidades

```
                    ┌─────────────────┐
                    │  multi-user.     │
                    │    target        │
                    └────────┬────────┘
                   Wants/Requires
            ┌────────┼────────┐
            │        │        │
     ┌──────▼──┐ ┌───▼───┐ ┌─▼──────┐
     │ sshd.   │ │nginx. │ │docker. │
     │ service │ │service│ │socket  │──── activa ───▶ docker.service
     └─────────┘ └───────┘ └────────┘

     apt-daily.timer ──── activa ───▶ apt-daily.service

     upload.path ──── activa ───▶ upload.service
```

Los servicios se agrupan en targets. Los sockets, timers y paths activan
servicios. Los slices controlan los recursos de los servicios.

---

## Ejercicios

### Ejercicio 1 — Explorar unidades

```bash
# 1. ¿Cuántos servicios están activos?
systemctl list-units --type=service --state=active --no-pager | tail -1

# 2. ¿Cuántos timers están configurados?
systemctl list-timers --no-pager

# 3. ¿Qué sockets están escuchando?
systemctl list-units --type=socket --state=active --no-pager
```

### Ejercicio 2 — Inspeccionar un servicio

```bash
# Elegir un servicio activo y examinarlo:
systemctl status sshd.service
systemctl show sshd.service --property=MainPID,ActiveState,SubState,Type

# ¿En qué slice está?
systemctl show sshd.service --property=Slice
```

### Ejercicio 3 — Relaciones

```bash
# Ver qué unidades dependen de network.target:
systemctl list-dependencies network.target --reverse

# Ver de qué depende sshd.service:
systemctl list-dependencies sshd.service
```
