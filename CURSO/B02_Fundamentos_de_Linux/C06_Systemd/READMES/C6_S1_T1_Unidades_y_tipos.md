# T01 — Unidades y tipos

> **Fuentes:** `README.md` + `README.max.md` + `labs/README.md`

## Erratas corregidas

| # | Ubicación | Error | Corrección |
|---|-----------|-------|------------|
| 1 | max:256 | Texto en chino: `"en桌面环境"` | `"en entornos de escritorio"` |
| 2 | max:270 | Texto en chino: `"que其他 targets usan"` | `"que otros targets usan"` |
| 3 | max:46 | Tabla: `"Activation por conexión de red"` | Typo → `"Activación"`; los sockets incluyen también Unix sockets, no solo red |
| 4 | max:52 | Tabla: `"no started by systemd"` (inglés mezclado) | `"no iniciados por systemd"` |
| 5 | max:186 | `"Soporta calendars complicated schedules"` (inglés) | `"Soporta calendarios con programación compleja"` |
| 6 | max:99 | `Type=exec` descrito como `"como simple pero sin forking"` | `exec` espera a que el binario se haya ejecutado exitosamente (`execve()` exitoso) antes de considerar el servicio iniciado |
| 7 | max:371 | cgroup path duplicado: `system.slice/system.slice/memory.max` | En cgroups v2 es `/sys/fs/cgroup/system.slice/memory.max` (sin duplicar) |
| 8 | max:511 | Texto garbled: `"mrz (mais recientes, recently)"` | Texto corrupto (mezcla portugués/inglés); eliminado |
| 9 | max:537 | `systemctl show apt-daily.timer -p Dependencies` | La propiedad `Dependencies` no existe; usar `-p Unit` o `systemctl cat` |

---

## Qué es una unidad (unit)

En systemd, **todo** recurso gestionado se representa como una **unidad** (unit).
Un servicio es una unidad, un punto de montaje es una unidad, un timer es una
unidad. El tipo se determina por la extensión del archivo:

```
Todo en systemd es una unidad:
  Un servicio (nginx)    → nginx.service
  Un temporizador        → apt-daily.timer
  Un punto de montaje    → home.mount
  Un swap                → dev-sda2.swap
  Un socket (SSH)        → sshd.socket
  Un target (runlevel)   → multi-user.target
```

### Dónde busca systemd los archivos de unidad

```
Rutas (en orden de precedencia):

/etc/systemd/system/         ← Administrador (overrides, unidades custom)
/run/systemd/system/         ← Runtime (creadas en ejecución, temporales)
/usr/lib/systemd/system/     ← Paquetes de la distro (instalados por rpm/dpkg)

Precedencia: /etc/ > /run/ > /usr/lib/
```

```bash
# Si creas una unidad en /etc/ con el mismo nombre que una en /usr/lib/,
# la de /etc/ tiene prioridad (permite override sin modificar archivos de distro)

# Ver qué archivo de unidad se está usando:
systemctl status nginx.service
#    Loaded: loaded (/usr/lib/systemd/system/nginx.service; enabled; ...)
#                    ↑ ruta del archivo activo
```

### Listar unidades

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

---

## Tipos de unidades

| Tipo | Extensión | Qué gestiona |
|---|---|---|
| **service** | `.service` | Procesos y daemons |
| **socket** | `.socket` | Activación por conexión (TCP, UDP, Unix socket) |
| **timer** | `.timer` | Tareas programadas (reemplazo de cron) |
| **target** | `.target` | Grupos de unidades (como runlevels) |
| **mount** | `.mount` | Puntos de montaje |
| **automount** | `.automount` | Montaje on-demand |
| **slice** | `.slice` | Control de recursos (cgroups) |
| **scope** | `.scope` | Procesos externos (no iniciados por systemd) |
| **path** | `.path` | Activación por cambios en filesystem |
| **swap** | `.swap` | Dispositivos de swap |
| **device** | `.device` | Dispositivos hardware (generados por kernel/udev) |

---

## service — Procesos y daemons

La unidad más común. Gestiona un proceso que systemd inicia, monitorea y puede
reiniciar:

```bash
# Ejemplos:
sshd.service          # servidor SSH
nginx.service         # servidor web
docker.service        # Docker daemon
cron.service          # cron daemon (Debian)
crond.service         # cron daemon (RHEL)

# El sufijo .service es el default — se puede omitir:
systemctl status sshd          # equivalente a sshd.service
systemctl status nginx         # equivalente a nginx.service
```

### Anatomía de un .service

```ini
# /etc/systemd/system/mi-servicio.service

[Unit]
Description=Mi aplicación
After=network.target

[Service]
Type=simple                    # cómo systemd inicia el proceso
ExecStart=/usr/local/bin/myapp # comando a ejecutar
Restart=on-failure             # política de reinicio
RestartSec=5                   # segundos antes de reiniciar

[Install]
WantedBy=multi-user.target     # qué target lo activa
```

### Tipos de servicio (Type=)

```bash
# Type=
#   simple   → ExecStart es el proceso principal (default)
#   exec     → como simple, pero systemd espera a que execve() tenga éxito
#              antes de considerar el servicio "iniciado"
#   forking  → ExecStart hace fork() y el padre termina (daemon clásico)
#   oneshot  → el proceso termina y el servicio queda "exited" (scripts de setup)
#   dbus     → espera a que el servicio adquiera un nombre en D-Bus
#   notify   → el proceso envía sd_notify() cuando está listo
#   idle     → espera a que no haya otros jobs activos
```

---

## socket — Activación por socket

Systemd escucha en un socket (TCP, UDP, Unix socket) y arranca el servicio
correspondiente cuando llega una conexión. El servicio no consume recursos
hasta que se necesita:

```
┌───────────────────────────────────────────────────────┐
│                  SIN socket activation                 │
│                                                       │
│   docker.service empieza con el sistema               │
│   → Consume memoria y CPU desde el arranque           │
│   → Aunque nadie use Docker durante días              │
├───────────────────────────────────────────────────────┤
│                  CON socket activation                 │
│                                                       │
│   docker.socket escucha en /var/run/docker.sock       │
│   → Apenas consume recursos (solo el socket)          │
│                                                       │
│   Cuando un usuario ejecuta: docker ps                │
│   → docker.socket recibe conexión                     │
│   → systemd arranca docker.service                    │
│   → El servicio maneja el request                     │
└───────────────────────────────────────────────────────┘
```

```bash
# Ejemplos:
sshd.socket             # activa sshd.service al recibir conexión en :22
cups.socket             # activa CUPS al recibir job de impresión
docker.socket           # activa Docker al usar /var/run/docker.sock
dbus.socket             # bus de mensajes del sistema

systemctl list-units --type=socket --state=active

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

Ventajas de activación por socket:
1. El servicio no arranca hasta que alguien se conecta
2. Boot más rápido (sockets listos inmediatamente, sin esperar al servicio)
3. Si el servicio crashea, el socket sigue escuchando y lo reactiva

---

## timer — Tareas programadas (reemplazo de cron)

Cada timer tiene un servicio asociado que ejecuta según un calendario:

```
┌───────────────────────────────────────────────────────┐
│                       cron                             │
│  • Sin control de estado (no sabe si el job anterior  │
│    sigue corriendo)                                   │
│  • No tiene dependencias (no puede esperar a la red)  │
│  • Logs mixeados en syslog/cron.log                   │
├───────────────────────────────────────────────────────┤
│                       timer                            │
│  • Puede esperar a la red o a otros servicios          │
│  • Logs integrados: journalctl -u servicio            │
│  • Persistent=true: ejecuta al arrancar si se perdió  │
│  • Visible con systemctl list-timers                  │
│  • No ejecuta si el anterior aún está corriendo       │
└───────────────────────────────────────────────────────┘
```

```bash
# Timers del sistema:
systemctl list-timers --no-pager
# NEXT                         LEFT        LAST                         PASSED    UNIT
# Mon 2024-03-18 06:00:00 UTC  12h left    Sun 2024-03-17 06:00:00 UTC 5h ago    apt-daily.timer
```

```ini
# Ejemplo: backup.timer
[Unit]
Description=Daily backup at 3am

[Timer]
OnCalendar=*-*-* 03:00:00       # calendario
Persistent=true                  # ejecutar al arrancar si se perdió
RandomizedDelaySec=60            # espera aleatoria 0-60s (evitar thundering herd)

[Install]
WantedBy=timers.target
```

### Formato OnCalendar

```bash
OnCalendar=daily              # una vez al día (medianoche)
OnCalendar=hourly             # cada hora
OnCalendar=*-*-01 00:00:00    # día 1 de cada mes a medianoche
OnCalendar=Mon *-*-* 09:00    # lunes a las 9am
OnCalendar=*:00/15            # cada 15 minutos
OnCalendar=2024-03-15 12:00   # fecha específica
```

---

## target — Grupos de unidades

Los targets agrupan unidades y definen estados del sistema. Son el reemplazo
de los runlevels de SysVinit:

```
Targets principales:

  poweroff.target      → Sistema apagado (runlevel 0)
  rescue.target        → Modo single-user (runlevel 1)
  multi-user.target    → Multiusuario sin GUI (runlevel 3)
  graphical.target     → Multiusuario con GUI (runlevel 5)
  reboot.target        → Reiniciar (runlevel 6)
```

```bash
# Target actual:
systemctl get-default
# multi-user.target  (o graphical.target en entornos de escritorio)

# Cambiar target por defecto:
sudo systemctl set-default multi-user.target
sudo systemctl set-default graphical.target

# Cambiar target en runtime (sin reiniciar):
sudo systemctl isolate multi-user.target
```

### Targets especiales del sistema

```bash
# Targets que otros targets usan como dependencias:
local-fs.target          # sistemas de archivos locales montados
remote-fs.target         # sistemas de archivos remotos montados
network.target           # red básica disponible
network-online.target    # red completamente configurada
sockets.target           # todos los sockets activos
sysinit.target           # inicialización del sistema
basic.target             # sistema básico funcional
timers.target            # todos los timers activos

systemctl list-dependencies multi-user.target
```

---

## mount y automount — Puntos de montaje

Systemd puede gestionar puntos de montaje, generados desde `/etc/fstab`
o creados como archivos `.mount`:

```bash
# El nombre de la unidad refleja la ruta con - en lugar de /:
# /home       → home.mount
# /var/log    → var-log.mount
# /boot/efi   → boot-efi.mount

systemctl list-units --type=mount
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
# El automount monta el filesystem al primer acceso
# y puede desmontarlo tras un período de inactividad

# Ejemplo: NFS on-demand desde /etc/fstab:
# server:/export /mnt/nfs nfs auto,nofail,x-systemd.automount 0 0
# Se convierte en: mnt-nfs.automount + mnt-nfs.mount
```

---

## slice — Control de recursos (cgroups)

Los slices organizan los procesos en una jerarquía de cgroups para controlar
recursos (CPU, memoria, I/O):

```
Jerarquía de slices:
                    -.slice (root)
                    │
          ┌─────────┴─────────┐
          │                   │
    system.slice         user.slice
          │                   │
    ┌─────┴─────┐       ┌────┴──────┐
 nginx  postgresql  user-1000.slice
                              │
                        gnome-session
                        terminal
```

```bash
# Slices predefinidos:
# -.slice          → root (padre de todos)
# system.slice     → servicios del sistema
# user.slice       → sesiones de usuario
# machine.slice    → contenedores y VMs

# Cada servicio pertenece a un slice:
systemctl status nginx.service
# ... CGroup: /system.slice/nginx.service

# Ver la jerarquía:
systemd-cgls

# Ver límites de un slice (cgroups v2):
cat /sys/fs/cgroup/system.slice/memory.max
```

```ini
# Ejemplo: limitar recursos de un slice
[Slice]
MemoryMax=2G
CPUQuota=50%
```

---

## scope — Procesos externos

Los scopes agrupan procesos que **no** fueron iniciados por systemd pero que
se quieren gestionar. Se crean automáticamente (no tienen archivos `.scope`):

```bash
# Ejemplos:
session-1.scope           # sesión de login (PAM)
init.scope                # PID 1
# machine-*.scope         # contenedores (systemd-nspawn)

systemctl list-units --type=scope
```

---

## path — Activación por cambio en filesystem

Monitorea una ruta y activa un servicio cuando se detecta un cambio:

```ini
# upload-watcher.path
[Unit]
Description=Watch for uploaded files

[Path]
DirectoryNotEmpty=/var/spool/uploads
MakeDirectory=true          # crear el directorio si no existe

[Install]
WantedBy=multi-user.target

# Cuando /var/spool/uploads deja de estar vacío,
# systemd arranca upload-watcher.service automáticamente
```

### Directivas de [Path]

```bash
PathExists=/path              # activar cuando la ruta existe
PathExistsGlob=/path/*        # activar cuando el glob tiene resultados
PathChanged=/path             # activar cuando el archivo cambia (close_write)
PathModified=/path            # activar cuando se modifica (incluye metadata)
DirectoryNotEmpty=/path       # activar cuando el directorio no está vacío
Unit=other.service            # qué servicio arrancar (default: mismo nombre)
```

---

## swap — Dispositivos de swap

```bash
# Generados automáticamente desde /etc/fstab:
systemctl list-units --type=swap
# dev-sda2.swap

# Nombre basado en la ruta del dispositivo:
# /dev/sda2 → dev-sda2.swap
```

---

## device — Dispositivos hardware

Unidades generadas automáticamente por systemd por cada dispositivo que el
kernel expone en sysfs/udev:

```bash
# No se crean con archivos — el kernel los genera automáticamente
systemctl list-units --type=device
```

---

## Relaciones entre tipos de unidades

```
                    ┌──────────────────────────────────────┐
                    │           basic.target               │
                    │  (sistema base: sysinit + sockets    │
                    │           + timers + paths)          │
                    └──────────────┬───────────────────────┘
                                   │
           ┌───────────────────────┼───────────────────────┐
           │                       │                       │
    multi-user.target      graphical.target        rescue.target
           │                       │
           │               display manager (gdm, lightdm)
           │
    ┌──────┴──────┐
    │             │
    │    docker.socket ── activa ──▶ docker.service
    │
    │    apt-daily.timer ── activa ──▶ apt-daily.service
    │
    │    home.automount ── activa (on-demand) ──▶ home.mount
    │
    └─ Cada servicio corre en un SLICE:
         nginx.service    → /system.slice/
         gnome-terminal   → /user.slice/user-1000.slice/
```

Los servicios se agrupan en targets. Los sockets, timers y paths activan
servicios. Los slices controlan los recursos de los servicios.

---

## Lab — Unidades y tipos

### Parte 1 — Explorar unidades instaladas

#### Paso 1.1: Listar unit files por tipo

```bash
echo "--- Servicios (.service) ---"
ls /usr/lib/systemd/system/*.service 2>/dev/null | wc -l
ls /usr/lib/systemd/system/*.service 2>/dev/null | head -10

echo "--- Sockets (.socket) ---"
ls /usr/lib/systemd/system/*.socket 2>/dev/null

echo "--- Timers (.timer) ---"
ls /usr/lib/systemd/system/*.timer 2>/dev/null

echo "--- Targets (.target) ---"
ls /usr/lib/systemd/system/*.target 2>/dev/null | wc -l
```

#### Paso 1.2: Contar unidades por tipo

Antes de ejecutar, predice: ¿qué tipo será el más numeroso? ¿Y el menos?

```bash
for ext in service socket timer target mount automount slice path swap; do
    COUNT=$(ls /usr/lib/systemd/system/*."$ext" 2>/dev/null | wc -l)
    printf "  %-12s %3d unidades\n" ".$ext" "$COUNT"
done
```

Los `.service` son la gran mayoría. Cada tipo tiene un propósito específico.

#### Paso 1.3: Nombres y extensiones

```bash
# El tipo se determina por la extensión:
ls /usr/lib/systemd/system/systemd-*.service 2>/dev/null | head -5

# Los mounts reflejan la ruta con - en lugar de /:
ls /usr/lib/systemd/system/*.mount 2>/dev/null | head -5
# /tmp → tmp.mount, /var/log → var-log.mount

# Targets principales:
ls /usr/lib/systemd/system/{multi-user,graphical,rescue,basic}.target 2>/dev/null
```

### Parte 2 — Inspeccionar unidades

#### Paso 2.1: Anatomía de un .service

```bash
# Buscar un servicio representativo:
cat /usr/lib/systemd/system/systemd-journald.service
```

Observa las tres secciones:
- `[Unit]` — metadata y dependencias (común a todos los tipos)
- `[Service]` — configuración específica del servicio
- `[Install]` — cómo se integra al boot (`enable`/`disable`)

#### Paso 2.2: Anatomía de un .socket

```bash
cat /usr/lib/systemd/system/systemd-journald.socket 2>/dev/null || \
    ls /usr/lib/systemd/system/*.socket 2>/dev/null | head -1 | xargs cat
```

Un `.socket` activa un `.service` cuando llega una conexión.
El servicio no consume recursos hasta que se necesita.

#### Paso 2.3: Anatomía de un .timer

```bash
cat /usr/lib/systemd/system/systemd-tmpfiles-clean.timer 2>/dev/null || \
    ls /usr/lib/systemd/system/*.timer 2>/dev/null | head -1 | xargs cat
```

Un `.timer` activa un `.service` según el calendario definido.
Es el reemplazo moderno de cron.

#### Paso 2.4: Anatomía de un .target

```bash
cat /usr/lib/systemd/system/multi-user.target
cat /usr/lib/systemd/system/rescue.target
```

Los targets son simples: solo `[Unit]` e `[Install]`.
Su función es **agrupar** otras unidades mediante `Wants=`/`Requires=`.

### Parte 3 — Relaciones entre tipos

#### Paso 3.1: Targets como agrupadores

```bash
# Los servicios declaran WantedBy=multi-user.target en [Install].
# Al hacer systemctl enable, se crea un symlink en:
ls -la /etc/systemd/system/multi-user.target.wants/ 2>/dev/null

# Otros directorios .wants/:
find /etc/systemd/system -name "*.wants" -type d 2>/dev/null
```

Cada directorio `.wants/` contiene symlinks a las unidades que se activan
cuando ese target se alcanza.

#### Paso 3.2: Socket activa service

```bash
# Convención: sshd.socket activa sshd.service (mismo nombre base)
for sock in /usr/lib/systemd/system/*.socket; do
    [[ -f "$sock" ]] || continue
    BASE=$(basename "$sock" .socket)
    SVC="/usr/lib/systemd/system/${BASE}.service"
    if [[ -f "$SVC" ]]; then
        echo "  $(basename "$sock") → ${BASE}.service"
    fi
done
```

#### Paso 3.3: Timer activa service

```bash
# Convención: backup.timer activa backup.service (mismo nombre base)
for tmr in /usr/lib/systemd/system/*.timer; do
    [[ -f "$tmr" ]] || continue
    BASE=$(basename "$tmr" .timer)
    SVC="/usr/lib/systemd/system/${BASE}.service"
    if [[ -f "$SVC" ]]; then
        echo "  $(basename "$tmr") → ${BASE}.service"
        grep -E "OnCalendar|OnBoot|OnUnitActive" "$tmr" 2>/dev/null | sed "s/^/      /"
    fi
done
```

---

## Ejercicios

### Ejercicio 1 — Contar unidades por tipo

```bash
systemctl list-units --type=service --state=active --no-pager | tail -1
systemctl list-units --type=socket --state=active --no-pager | tail -1
systemctl list-units --type=mount --state=active --no-pager | tail -1
systemctl list-units --type=timer --state=active --no-pager | tail -1
```

¿Qué tipo tiene más unidades activas?

<details><summary>Predicción</summary>

`.service` será el tipo más numeroso por un amplio margen (típicamente
30-60+ servicios activos). Los `.mount` suelen ser el segundo (10-20,
uno por cada punto de montaje). `.socket` y `.timer` tienen pocas unidades
(5-15 cada uno).

`tail -1` muestra la línea de resumen al final de la lista, que indica
el conteo total (ej: `42 loaded units listed`).

</details>

### Ejercicio 2 — Inspeccionar un servicio

```bash
systemctl status sshd.service
systemctl show sshd.service --property=MainPID,ActiveState,SubState,Type
systemctl show sshd.service --property=Slice
```

¿En qué slice está `sshd`? ¿Qué tipo de servicio es (`Type=`)?

<details><summary>Predicción</summary>

```
MainPID=XXXX
ActiveState=active
SubState=running
Type=notify       (o Type=exec según la distro)
Slice=system.slice
```

`sshd` está en `system.slice` porque es un servicio del sistema (no un
proceso de usuario). `Type=notify` significa que sshd envía una señal
`sd_notify()` a systemd cuando está listo para aceptar conexiones.
En algunas distros puede ser `Type=exec` o `Type=forking`.

</details>

### Ejercicio 3 — Dependencias de un target

```bash
# ¿Qué unidades necesita multi-user.target?
systemctl list-dependencies multi-user.target

# ¿Qué unidades dependen de network.target?
systemctl list-dependencies network.target --reverse
```

¿Qué muestra `--reverse` y por qué es útil?

<details><summary>Predicción</summary>

Sin `--reverse`, `list-dependencies` muestra las unidades que el target
**necesita** (sus dependencias hacia abajo). `multi-user.target` depende
de `basic.target`, que depende de `sysinit.target`, `sockets.target`, etc.

Con `--reverse`, muestra qué unidades **dependen de** `network.target`
(dependencias hacia arriba): servicios como `sshd.service`, `nginx.service`,
etc. que declaran `After=network.target`.

`--reverse` responde a la pregunta: "¿qué se rompería si esta unidad no
estuviera disponible?"

</details>

### Ejercicio 4 — Pares socket/service

```bash
# Listar sockets activos:
systemctl list-units --type=socket --state=active --no-pager

# Para cada socket, verificar si su servicio está activo o inactivo:
systemctl status sshd.socket 2>/dev/null
systemctl status sshd.service 2>/dev/null
```

¿Puede `sshd.socket` estar activo mientras `sshd.service` está inactivo?

<details><summary>Predicción</summary>

**Sí.** Ese es el punto de la activación por socket:
- `sshd.socket` está activo y escuchando en el puerto 22
- `sshd.service` puede estar inactivo (sin consumir recursos)
- Cuando llega una conexión SSH, systemd arranca `sshd.service`
  automáticamente

Sin embargo, en la mayoría de las instalaciones por defecto, `sshd.service`
está habilitado directamente (sin socket activation) y aparece como activo.
Socket activation es una opción, no un requisito.

Si ambos están habilitados, systemd prefiere el servicio directo.

</details>

### Ejercicio 5 — Explorar timers

```bash
systemctl list-timers --all --no-pager

# Ver el contenido de un timer:
systemctl cat apt-daily.timer 2>/dev/null || \
    systemctl cat systemd-tmpfiles-clean.timer 2>/dev/null
```

¿Qué servicio activa el timer? ¿Qué hace `Persistent=true`?

<details><summary>Predicción</summary>

`systemctl list-timers` muestra: NEXT (próxima ejecución), LEFT (cuánto
falta), LAST (última ejecución), PASSED (hace cuánto), UNIT (el timer).

El timer activa un servicio con el **mismo nombre base**: `apt-daily.timer`
activa `apt-daily.service`.

`Persistent=true` significa: si la máquina estaba apagada cuando el timer
debía disparar, systemd ejecuta el servicio inmediatamente al arrancar.
Sin `Persistent=true`, la ejecución perdida se ignora.

`systemctl cat` muestra el contenido del unit file — es preferible a
buscar el archivo manualmente porque considera la precedencia de rutas.

</details>

### Ejercicio 6 — Jerarquía de slices

```bash
# Ver la jerarquía de cgroups:
systemd-cgls | head -40

# Ver el slice de un servicio:
systemctl show sshd.service --property=Slice

# Ver límites de memoria (cgroups v2):
cat /sys/fs/cgroup/system.slice/memory.max
```

¿Qué valor tiene `memory.max` para `system.slice`?

<details><summary>Predicción</summary>

`memory.max` probablemente muestra `max` (sin límite), porque por defecto
los slices no tienen restricciones de recursos — toda la memoria del
sistema está disponible.

`systemd-cgls` muestra la jerarquía como un árbol:
```
Control group /:
├─init.scope
│ └─1 /usr/lib/systemd/systemd
├─system.slice
│ ├─sshd.service
│ │ └─XXXX sshd: /usr/sbin/sshd
│ ├─nginx.service
│ └─...
└─user.slice
  └─user-1000.slice
    └─session-1.scope
```

Los servicios del sistema están en `system.slice`, las sesiones de usuario
en `user.slice/user-UID.slice`.

</details>

### Ejercicio 7 — Rutas de archivos y precedencia

```bash
# Ver la ruta del archivo de unidad:
systemctl status sshd.service | grep Loaded

# ¿Hay overrides en /etc/?
ls -la /etc/systemd/system/sshd.service.d/ 2>/dev/null

# Ver la configuración efectiva (con overrides aplicados):
systemctl cat sshd.service
```

¿Qué diferencia hay entre `systemctl cat` y leer el archivo directamente?

<details><summary>Predicción</summary>

`systemctl cat` muestra **todos** los fragmentos de configuración en orden
de precedencia:
1. El archivo base (ej: `/usr/lib/systemd/system/sshd.service`)
2. Cualquier drop-in override (ej: `/etc/systemd/system/sshd.service.d/override.conf`)

Cada fragmento se muestra precedido por `# /ruta/al/archivo`.

Leer el archivo directamente (`cat /usr/lib/systemd/system/sshd.service`)
solo muestra la configuración base, sin los overrides que el administrador
haya podido agregar.

</details>

### Ejercicio 8 — Explorar mounts

```bash
systemctl list-units --type=mount --no-pager

# Ver el contenido de un mount:
systemctl cat tmp.mount 2>/dev/null
```

Si `/var/log` está montado, ¿cómo se llamará la unidad mount?

<details><summary>Predicción</summary>

`var-log.mount`. Regla de conversión: cada `/` se reemplaza por `-` en el
nombre de la unidad, y se omite el `/` inicial.

Ejemplos:
- `/home` → `home.mount`
- `/var/log` → `var-log.mount`
- `/boot/efi` → `boot-efi.mount`
- `/mnt/data` → `mnt-data.mount`

Las unidades mount se generan automáticamente desde `/etc/fstab`.
También se pueden crear manualmente como archivos `.mount`.

</details>

### Ejercicio 9 — Path activation

```bash
# Listar paths activos (si hay):
systemctl list-units --type=path --no-pager

# Buscar archivos .path instalados:
ls /usr/lib/systemd/system/*.path 2>/dev/null
```

¿Cuál es la diferencia entre `PathChanged` y `PathModified`?

<details><summary>Predicción</summary>

Ambos monitoran cambios en un archivo, pero difieren en cuándo disparan:

- `PathChanged` → dispara cuando el archivo se cierra tras escribir en él
  (evento `close_write` de inotify). Solo cambios de contenido.
- `PathModified` → dispara cuando el archivo se modifica, incluyendo
  cambios de metadata (permisos, timestamps, etc.) además de contenido.

`PathChanged` es más específico y genera menos activaciones falsas.
`PathModified` cubre más casos pero puede disparar innecesariamente
(ej: un `chmod` sin cambiar contenido).

Otras directivas:
- `PathExists` → dispara cuando la ruta aparece
- `DirectoryNotEmpty` → dispara cuando el directorio deja de estar vacío

</details>

### Ejercicio 10 — Mapa completo de relaciones

```bash
# Ver qué unidades quiere multi-user.target:
systemctl list-dependencies multi-user.target --no-pager | head -20

# Ver qué sockets activan qué servicios:
for sock in $(systemctl list-units --type=socket --state=active \
    --plain --no-legend --no-pager | awk '{print $1}'); do
    SVC="${sock%.socket}.service"
    STATE=$(systemctl is-active "$SVC" 2>/dev/null)
    echo "  $sock → $SVC ($STATE)"
done
```

¿Por qué algunos servicios asociados a sockets están "inactive"?

<details><summary>Predicción</summary>

Un servicio asociado a un socket está "inactive" porque **aún no se ha
necesitado**: nadie se ha conectado al socket todavía.

Ejemplo típico:
```
  docker.socket → docker.service (inactive)
```
Docker no está corriendo porque nadie ha ejecutado un comando `docker`
todavía. Cuando alguien lo haga, la conexión al socket activará el servicio.

```
  sshd.socket → sshd.service (active)
```
SSH ya tiene conexiones activas, así que el servicio está corriendo.

Esta es la esencia de la activación por socket: los servicios solo
consumen recursos cuando realmente se necesitan.

</details>
