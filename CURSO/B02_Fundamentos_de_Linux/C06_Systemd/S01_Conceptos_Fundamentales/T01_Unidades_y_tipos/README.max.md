# T01 — Unidades y tipos

## Qué es una unidad (unit)

En systemd, **todo** recurso gestionado por el sistema se representa como una **unidad** (unit). Una unidad es simplemente un archivo de configuración que le dice a systemd cómo gestionar un recurso específico.

```
Todo en systemd es una unidad:
  Un servicio (nginx)    → nginx.service
  Un temporizador (cron) → apt-daily.timer
  Un punto de montaje    → home.mount
  Un swap               → dev-sda2.swap
  Un socket (SSH)       → sshd.socket
  Un target (runlevel)  → multi-user.target
```

Systemd lee las unidades de estos directorios:

```
Rutas donde systemd busca archivos de unidad:

/usr/lib/systemd/system/     ← Unidades instaladas por paquetes (distro)
/run/systemd/system/          ← Unidades runtime (creadas en ejecución)
/etc/systemd/system/         ← Unidades administradas por el administrador
                                (overrides, unidades custom)
```

```bash
# Precedencia: /etc/ > /run/ > /usr/lib/
# Si creas una unidad en /etc/ con el mismo nombre que una en /usr/lib/,
# la de /etc/ toma precedence (permite override sin modificar archivos de distro)

# Ver qué archivo de unidad se está usando
systemctl status nginx.service
#    Loaded: loaded (/etc/systemd/system/nginx.service; enabled; ...)
#          ↑ Aquí se ve cuál tiene prioridad
```

## Tipos de unidades

Systemd soporta múltiples tipos de unidades, cada una para un propósito diferente:

| Tipo | Extensión | Qué gestiona |
|---|---|---|
| **service** | `.service` | Procesos y daemons |
| **socket** | `.socket` | Activation por conexión de red |
| **timer** | `.timer` | Tareas programadas (reemplazo de cron) |
| **target** | `.target` | Grupos de unidades (como runlevels) |
| **mount** | `.mount` | Puntos de montaje |
| **automount** | `.automount` | Montaje on-demand |
| **slice** | `.slice` | Control de recursos (cgroups) |
| **scope** | `.scope` | Procesos externos (no started by systemd) |
| **path** | `.path` | Activación por cambios en filesystem |
| **swap** | `.swap` | Dispositivos de swap |
| **device** | `.device` | Dispositivos hardware (kernel) |

## service — Procesos y daemons

La unidad más común. Gestiona un proceso que systemd inicia, monitorea y puede reiniciar:

```bash
# Ejemplos de servicios del sistema:
systemctl list-units --type=service --state=active | head -20
# sshd.service         ← servidor SSH
# nginx.service        ← servidor web
# docker.service       ← daemon de Docker
# systemd-resolved    ← resolver DNS
# cron.service         ← planificador de tareas (Debian)
# crond.service        ← planificador de tareas (RHEL)

# El sufijo .service es el DEFAULT — se puede omitir en systemctl:
systemctl status sshd          # equivalente a sshd.service
systemctl status nginx         # equivalente a nginx.service
```

### Anatomía de un .service

```ini
# /etc/systemd/system/mi-servicio.service

[Unit]
Description=Mi aplicación
# Documentación: man systemd.special(7)

[Service]
Type=simple                    # cómo systemd inicia el proceso
ExecStart=/usr/local/bin/miapp # comando a ejecutar
Restart=on-failure             # política de reinicio
RestartSec=5                  # segundos antes de reiniciar

[Install]
WantedBy=multi-user.target     # qué target lo activa
```

```bash
# Los campos principales de [Service]:
# Type=
#   simple      → ExecStart es el proceso principal
#   exec        → como simple pero sin forking
#   forking     → ExecStart hace fork() (daemon clásico)
#   oneshot     → el proceso termina (ej: scripts de setup)
#   dbus        → necesita un BusName (espera al bus)
#   notify      → el proceso envía sd_notify() cuando está listo
#   idle        → espera a que el sistema esté idle
```

## socket — Activación por socket

Systemd escucha en un socket (TCP, Unix socket) y cuando recibe una conexión, arranca el servicio correspondiente. El servicio no consume recursos hasta que se necesita:

```
┌─────────────────────────────────────────────────────────────┐
│                    SIN socket activation                      │
│                                                              │
│   docker.service empieza con el sistema                      │
│   → Consume memoria y CPU desde el arranque                  │
│   → Aunque nadie use Docker durante días                     │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                   CON socket activation                      │
│                                                              │
│   docker.socket escucha en /var/run/docker.sock              │
│   → Apenas consume recursos (solo el socket)                 │
│                                                              │
│   Cuando un usuario ejecuta: docker ps                        │
│   → docker.socket recibe conexión                            │
│   → systemd arranca docker.service                           │
│   → El servicio maneja el request                            │
│   → Si no hay más requests, el servicio puede morir         │
│     (según configuración)                                   │
└─────────────────────────────────────────────────────────────┘
```

```bash
# Ejemplos de sockets del sistema:
systemctl list-units --type=socket --state=active
# sshd.socket    → arranca sshd.service cuando alguien se conecta al :22
# cups.socket    → arranca cups.service cuando llega job de impresión
# docker.socket  → arranca docker.service cuando se usa docker CLI
# dbus.socket    → el bus de mensajes del sistema

# Ver sockets escuchando
ss -ltn | grep LISTEN
# Netid  State   Recv-Q  Send-Q  Local Address:Port  Peer Address:Port
# tcp    LISTEN  0       128     0.0.0.0:22         0.0.0.0:*
# tcp    LISTEN  0       128     [::]:22            [::]:*

# Si sshd.socket está activo, sshd.service puede estar INACTIVO
# hasta que llegue la primera conexión
systemctl status sshd.socket
systemctl status sshd.service
```

```ini
# Ejemplo: sshd.socket
[Unit]
Description=OpenSSH Server Socket

[Socket]
ListenStream=22
Accept=yes       # cada conexión crea un proceso sshd separado
                # Accept=no → una sola instancia recibe todas las conexiones

[Install]
WantedBy=sockets.target
```

## timer — Tareas programadas (reemplazo de cron)

Los timers son el reemplazo moderno de cron en systemd. Tienen ventajas sobre cron:

```
┌─────────────────────────────────────────────────────────────┐
│                         cron                                │
│  • Sin control de estado (no sabe si el job anterior sigue  │
│    corriendo)                                               │
│  • No hay dependencias (no puede esperar a la red)         │
│  • Logs van a /var/log/syslog o cron.log (mixeado)         │
│  • No se pueden programar eventos one-shot fácilmente        │
├─────────────────────────────────────────────────────────────┤
│                         timer                               │
│  • systemd inicia el .service asociado cuando corresponde  │
│  • Puede esperar a que la red esté disponible               │
│  • Logs van al journal (journalctl -u service)             │
│  • Soporta calendars complicated schedules                  │
│  • Puede hacer que un job no se ejecute si el anterior aún │
│    está corriendo                                           │
│  • Visible con systemctl list-timers                       │
└─────────────────────────────────────────────────────────────┘
```

```bash
# Timers del sistema:
systemctl list-timers --no-pager
# NEXT                         LEFT        LAST                         PASSED    UNIT
# Mon 2024-03-18 06:00:00 UTC  12h left    Sun 2024-03-17 06:00:00 UTC 5h ago    apt-daily.timer
# Mon 2024-03-18 00:00:00 UTC  6h left     Sun 2024-03-17 00:00:00 UTC 11h ago   logrotate.timer

# Timers en detalle
systemctl status apt-daily.timer
systemctl status fstrim.timer      # TRIM para SSDs

# ¿Cuándo se ejecuta un timer?
systemctl show apt-daily.timer --property=NextElapseUSecRealtime
```

```ini
# Ejemplo: backup.timer
[Unit]
Description=Daily backup at 3am

[Timer]
# Calendar event: daily at 3am
OnCalendar=*-*-* 03:00:00
# Si el sistema estuvo apagado a las 3am, ejecutar al arrancar
Persistent=true
# Randóm: esperar 0-60min antes de ejecutar (evitar thundering herd)
RandomizedDelaySec=60

[Install]
WantedBy=timers.target
```

```bash
# Formato OnCalendar:
OnCalendar=daily              # una vez al día (medianoche)
OnCalendar=hourly            # cada hora
OnCalendar=*-*-01 00:00:00   # día 1 de cada mes a medianoche
OnCalendar=Mon *-*-* 09:00    # lunes a las 9am
OnCalendar=*:00/15            # cada 15 minutos
OnCalendar=2024-03-15 12:00   # fecha específica
```

## target — Grupos de unidades (como runlevels)

Los targets agrupan múltiples unidades y representan un estado del sistema. Son el equivalente systemd de los runlevels de SysVinit:

```
┌─────────────────────────────────────────────────────────────┐
│                    Targets principales                       │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  poweroff.target      → Sistema apagado (runlevel 0)        │
│  rescue.target        → Modo single-user (runlevel 1)       │
│  multi-user.target   → Multiusuario sin GUI (runlevel 3)   │
│  graphical.target    → Multiusuario con GUI (runlevel 5)   │
│  reboot.target       → Reiniciar (runlevel 6)               │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

```bash
# Target actual:
systemctl get-default
# multi-user.target  (o graphical.target en桌面环境)

# Cambiar target por defecto:
sudo systemctl set-default multi-user.target
sudo systemctl set-default graphical.target

# Cambiar target en runtime (sin reiniciar):
sudo systemctl isolate multi-user.target
sudo systemctl isolate graphical.target
```

### Targets especiales del sistema

```bash
# Todos los targets "básicos" que其他 targets usan:
systemctl list-units --type=target | grep -E "basic|sysinit|network|sockets|local"
# local-fs.target        → sistemas de archivos locales montados
# local-fs-pre.target    → antes de montar fs locales
# remote-fs.target       → sistemas de archivos remotos montados
# network.target         → red básica disponible
# network-online.target  → red completamente configurada
# network-pre.target     → antes de que la red esté disponible
# sockets.target         → todos los sockets activos
# sysinit.target        → inicialización del sistema
# basic.target          → sistema básico funcional
# timers.target         → todos los timers activos
```

## mount y automount — Puntos de montaje

Systemd puede gestionar puntos de montaje. Las unidades se generan automáticamente desde `/etc/fstab` o se crean manualmente:

```bash
# Convenciones de nombres:
# /home         → home.mount
# /var/log      → var-log.mount
# /boot/efi     → boot-efi.mount
# (la barra / se convierte en - en el nombre)

# Mounts activos:
systemctl list-units --type=mount
# home.mount    ← /home
# tmp.mount     ← /tmp
# boot-efi.mount ← /boot/efi

# Mounts definidos en /etc/fstab se generan automáticamente
# Las líneas en /etc/fstab se convierten en unidades .mount
```

```ini
# Ejemplo: tmp.mount
[Unit]
Description=Temporary Directory /tmp
Before=local-fs.target

[Mount]
What=tmpfs
Where=/tmp
Type=tmpfs
Options=mode=1777,strictatime,nosuid,nodev

[Install]
WantedBy=local-fs.target
```

### automount — Montaje on-demand

```bash
# home.automount monta /home cuando alguien intenta acceder a él
# y lo desmonta después de un período de inactividad

# Ejemplo: acceso NFS on-demand
# /etc/fstab:
# server:/export /mnt/nfs nfs auto,nofail,x-systemd.automount 0 0
# Se convierte en:
# mnt-nfs.automount + mnt-nfs.mount
```

## slice — Control de recursos (cgroups v2)

Los slices organizan procesos en una jerarquía de cgroups para limitar y controlar recursos:

```
Jerarquía de slices:
                    -.slice (root)
                    │
          ┌─────────┴─────────┐
          │                   │
    system.slice         user.slice
          │                   │
    ┌─────┴─────┐       ┌─────┴─────┐
 nginx  postgresql  docker  user-1000.slice
                                    │
                               gnome-session
                               terminal
```

```bash
# Ver la jerarquía de slices
systemd-cgls

# Cada servicio está en un slice:
systemctl status nginx
# → CGroup: /system.slice/nginx.service

# Slices por defecto:
# -.slice          → root (no tiene límite, padre de todos)
# system.slice     → servicios del sistema (límites por defecto)
# user.slice       → sesiones de usuario (límites por defecto)
# machine.slice    → contenedores y VMs
```

```bash
# Ver límites de un slice
systemctl status system.slice
cat /sys/fs/cgroup/system.slice/system.slice/memory.max

# Limitado un servicio a recursos específicos:
# Ver propiedades de CPU y memoria
systemctl show nginx.service --property=CPUAccounting,CPUWeight,MemoryAccounting,MemoryMax
```

## scope — Procesos externos

Los scopes agrupan procesos que **no** fueron iniciados por systemd pero se quieren gestionar como grupo. Se crean automáticamente:

```bash
# Scopes del sistema:
systemctl list-units --type=scope
# session-1.scope    → sesión de usuario 1 (PAM)
# session-2.scope    → sesión de usuario 2
# init.scope         → PID 1 (no es realmente un scope pero se muestra)
# machine-*.scope     → contenedores (systemd-nspawn)

# No tienen archivos .scope — se crean dinámicamente
```

## path — Activación por cambio en filesystem

Monitorea rutas y arranca servicios cuando detecta cambios:

```bash
# Ejemplo: vigilar un directorio de uploads
systemctl list-units --type=path
```

```ini
# upload-watcher.path
[Unit]
Description=Watch for uploaded files

[Path]
# Activa cuando el directorio no está vacío
DirectoryNotEmpty=/var/spool/uploads
MakeDirectory=true          # crear el directorio si no existe

[Install]
WantedBy=multi-user.target

# upload-watcher.service (asociado)
[Unit]
Description=Process uploaded files

[Service]
ExecStart=/usr/local/bin/process-uploads
Type=oneshot
```

```bash
# Directivas de [Path]:
PathExists=/path              # la ruta existe
PathExistsGlob=/path/*        # el glob tiene resultados
PathChanged=/path             # archivo cambió
PathModified=/path            # archivo se modificó
DirectoryNotEmpty=/path        # directorio dejó de estar vacío
Unit=other.service            # qué servicio arrancar
```

## swap — Dispositivos de swap

```bash
# Units swap generadas desde /etc/fstab:
systemctl list-units --type=swap
# dev-sda2.swap   → /dev/sda2 es swap
# dev-sdb1.swap   → /dev/sdb1 es swap

# Nombre: la ruta del dispositivo con / replaced por -
# /dev/sda2 → dev-sda2.swap
# /dev/mapper/vg-swap → dev-mapper-vg\\x2dswap.swap
```

## device — Dispositivos hardware

Los devices son unidades generadas automáticamente por systemd por cada dispositivo que el kernel expone en sysfs/udev:

```bash
# No se crean con archivos — el kernel los crea automáticamente
systemctl list-units --type=device
# sys-devices-LINUX:i386:00:03.0:vc:00:vtconsole:0.device
# dev-ttyS0.device
# dev-null.device
```

## Resumen: relaciones entre tipos de unidades

```
                    ┌──────────────────────────────────────┐
                    │           basic.target               │
                    │  (sistema base: sysinit + sockets   │
                    │           + timers + paths)          │
                    └──────────────┬───────────────────────┘
                                   │
           ┌───────────────────────┼───────────────────────┐
           │                       │                       │
    multi-user.target      graphical.target        rescue.target
           │                       │                       │
           │               ┌───────┴───────┐               │
           │               │               │               │
    services...      gdm.service     lightdm.service   │
           │                                               │
    timers...        (display manager)                   │
           │                                               │
    paths...                                              │
           │                                               │
    sockets...                                            │
           │                                               │
    ┌──────┴──────┐
    │             │
    │    docker.socket ── activa ──▶ docker.service
    │
    │    apt-daily.timer ── activa ──▶ apt-daily.service
    │
    │    home.automount ── activa (on-demand) ──▶ home.mount
    │
    └─ Cada servicio corre en un SLICE:
         nginx.service   → /system.slice/
         gnome-terminal → /user.slice/user-1000.slice/
```

---

## Ejercicios

### Ejercicio 1 — Explorar unidades cargadas

```bash
# 1. Listar TODAS las unidades activas
systemctl list-units --no-pager | head -30

# 2. Contar por tipo
systemctl list-units --type=service --state=active --no-pager | tail -1
systemctl list-units --type=socket --state=active --no-pager | tail -1
systemctl list-units --type=mount --state=active --no-pager | tail -1
systemctl list-units --type=timer --state=active --no-pager | tail -1

# 3. Ver unidades "mrz" (mais recientes, recently)
systemctl list-units --type=service --state=active --plain --no-pager | \
    sort -k3 | tail -10

# 4. Ver propiedades de una unidad
systemctl show sshd.service -p Loaded,Active,SubState
```

### Ejercicio 2 — Inspeccionar tipos de unidades

```bash
# 1. Elegir un servicio y ver sus propiedades
systemctl status cron.service   # Debian
systemctl status crond.service  # RHEL

# 2. Ver en qué target está este servicio
systemctl show cron.service -p WantedBy

# 3. Ver el slice de un servicio
systemctl show cron.service -p Slice

# 4. Ver timers activos
systemctl list-timers --no-pager

# 5. Ver qué servicio activa un timer
systemctl show apt-daily.timer -p Dependencies
```

### Ejercicio 3 — Jerarquía de dependencias

```bash
# 1. Ver qué unidades necesita multi-user.target
systemctl list-dependencies multi-user.target

# 2. Ver qué target requiere network-online.target
systemctl list-dependencies network-online.target --reverse

# 3. Ver el árbol completo de dependencias de sshd
systemctl list-dependencies sshd.service

# 4. Visualizar con graphviz (si está instalado)
# systemctl dot | dot -Tsvg > deps.svg
```

### Ejercicio 4 — Explorar slices y recursos

```bash
# 1. Ver la jerarquía de slices
systemd-cgls | head -40

# 2. Ver los slices de usuario
systemd-cgls user.slice

# 3. Ver el slice de un servicio
systemctl status nginx.service | grep "CGroup"

# 4. Ver límites de memoria de system.slice
cat /sys/fs/cgroup/system.slice/memory.max

# 5. Comparar con un slice de usuario
cat /sys/fs/cgroup/user.slice/user-1000.slice/memory.max
```

### Ejercicio 5 — Archivos de unidad y precedencia

```bash
# 1. Ver la ruta de una unidad
systemctl status nginx.service | grep Loaded
# Loaded: loaded (/usr/lib/systemd/system/nginx.service; ...

# 2. ¿Hay override en /etc/?
ls -la /etc/systemd/system/nginx.service.d/

# 3. Crear un override temporal para ver cómo funciona
sudo systemctl edit nginx.service --drop-in=override
# Agregar:
# [Service]
# Restart=always

# 4. Ver el archivo override creado
cat /etc/systemd/system/nginx.service.d/override.conf

# 5. Recargar systemd
sudo systemctl daemon-reload
sudo systemctl restart nginx.service

# 6. Limpiar
sudo rm /etc/systemd/system/nginx.service.d/override.conf
sudo systemctl daemon-reload
```

### Ejercicio 6 — Socket y timer activation

```bash
# 1. ¿Está sshd.socket activo?
systemctl status sshd.socket

# 2. ¿Está sshd.service activo?
systemctl status sshd.service

# 3. Si sshd.service está "inactivo" pero sshd.socket está "activo",
#    sshd se arrancará en la primera conexión

# 4. Desactivar socket, activar service
sudo systemctl disable --now sshd.socket
sudo systemctl enable --now sshd.service

# 5. Ver qué timer ejecuta qué servicio
systemctl list-timers --all --no-pager | grep -E "daily|timer|upgrade"
```
