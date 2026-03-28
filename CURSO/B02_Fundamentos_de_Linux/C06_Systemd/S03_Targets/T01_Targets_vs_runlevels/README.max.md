# T01 — Targets vs runlevels

## Runlevels — El modelo SysVinit (legacy)

En el antiguo SysVinit, el sistema operaba en **runlevels** numerados. Cada runlevel era un estado fijo del sistema:

```
┌─────────────────────────────────────────────────────────────┐
│                     Runlevels SysVinit                        │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Runlevel 0 → poweroff.target → APAGAR                    │
│                                                              │
│  Runlevel 1 → rescue.target → MODO SINGLE USER             │
│              (root shell, sin red, reparación)             │
│                                                              │
│  Runlevel 2 → multi-user.target → Multiuser SIN GUI        │
│              (Debian: este es el "normal")                │
│                                                              │
│  Runlevel 3 → multi-user.target → Multiuser SIN GUI        │
│              (Red Hat: este es el "normal")               │
│                                                              │
│  Runlevel 4 → multi-user.target → RESERVADO                 │
│              (custom, no usado por defecto)                │
│                                                              │
│  Runlevel 5 → graphical.target → Multiuser CON GUI          │
│              (X11/Wayland, display manager)               │
│                                                              │
│  Runlevel 6 → reboot.target → REINICIAR                   │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

```bash
# Comandos legacy (todavía funcionan por compatibilidad):
runlevel           # N 5 → anterior=N (none), actual=5
init 3            # cambiar a runlevel 3
telinit 3         # equivalente

# Servicios en /etc/init.d/ y symlinks en /etc/rc{0-6}.d/:
ls /etc/rc3.d/
# S10syslog    → Start syslog (S = Start, 10 = orden)
# S20network   → Start network
# K30sshd      → Kill sshd si se sale del runlevel (K = Kill)
```

### Los 5 problemas del modelo SysVinit

```
1. SOLO UN RUNLEVEL activo — no se pueden combinar estados
   → "Quiero multi-user pero sin cron" → no es posible

2. ARRANQUE SECUENCIAL — S10 espera a que termine S09
   → Sin paralelismo → boot lento

3. DEPENDENCIAS NUMÉRICAS — el número define el orden
   → "S10network" debería arrancar antes que S20sshd
   → Pero no hay forma de decir "sshd necesita network"
   → Solo por convención de números

4. SIN DEPENDENCIAS DECLARATIVAS
   → El orden es implícito en los números
   → Agregar un servicio requiere crear 6 symlinks (uno por runlevel)

5. NO REUTILIZABLE
   → Runlevels 2, 3, 4 son "parecidos" pero duplicados
   →维护负担大
```

## Targets — El modelo systemd

Los targets son unidades que **agrupan otras unidades** mediante dependencias. Son el reemplazo moderno de los runlevels:

```
┌─────────────────────────────────────────────────────────────┐
│                     Targets systemd                          │
│                                                              │
│  Un target es una UNIDAD que agrupa servicios             │
│  Múltiples targets pueden estar activos simultáneamente   │
│  El paralelismo es automático según dependencias           │
│  Se pueden crear targets CUSTOM sin límite                │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

```bash
# Ver TODOS los targets activos (¡hay varios a la vez!):
systemctl list-units --type=target --state=active --no-pager
# UNIT                  LOAD   ACTIVE SUB    DESCRIPTION
# basic.target         loaded active active  Basic System
# local-fs.target     loaded active active  Local File Systems (pre-mounted)
# local-fs.target     loaded active active  Local File Systems
# multi-user.target    loaded active active  Multi-User System
# network.target       loaded active active  Network
# network-online.target loaded active active-waiting Network is Online
# rescue.target        loaded active active  Rescue Mode
# ...
```

## Mapeo runlevels → targets

```
┌─────────────────────────────────────────────────────────────┐
│              COMPARACIÓN Runlevel vs Target                   │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Runlevel 0  →  poweroff.target      (apagar)             │
│  Runlevel 1  →  rescue.target        (single user)         │
│  Runlevel 2  →  )                                           │
│  Runlevel 3  →  ) multi-user.target (multiuser, sin GUI)  │
│  Runlevel 4  →  )                                           │
│  Runlevel 5  →  graphical.target   (multiuser + GUI)       │
│  Runlevel 6  →  reboot.target      (reiniciar)            │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

```bash
# systemd mantiene aliases de compatibilidad:
ls -la /usr/lib/systemd/system/runlevel*.target
# runlevel0.target    → /usr/lib/systemd/system/poweroff.target
# runlevel1.target    → /usr/lib/systemd/system/rescue.target
# runlevel2.target    → /usr/lib/systemd/system/multi-user.target
# runlevel3.target    → /usr/lib/systemd/system/multi-user.target
# runlevel4.target    → /usr/lib/systemd/system/multi-user.target
# runlevel5.target    → /usr/lib/systemd/system/graphical.target
# runlevel6.target    → /usr/lib/systemd/system/reboot.target

# Los comandos legacy siguen funcionando:
runlevel
# N 3  → runlevel anterior=N (none), actual=3
```

```bash
# Importante diferencia:
# En SysV: runlevel 2 = sin red, runlevel 3 = con red (en algunas distros)
# En systemd: la red es INDEPENDIENTE del runlevel/target
# → Se configura con network.target / network-online.target
# → Un mismo multi-user.target puede tener o no tener red
```

## Targets principales del sistema

### Targets de transición del sistema

```bash
# Apagar:
sudo systemctl poweroff
# Equivalente a: init 0, shutdown -h now

# Reiniciar:
sudo systemctl reboot
# Equivalente a: init 6, shutdown -r now

# Halt (solo detiene, no apaga hardware):
sudo systemctl halt
# Equivalente a: halt

# Apagar y cortar energía (poweroff):
sudo systemctl poweroff --force
# Fuerza el apagado sin llamar a los servicios de shutdown
```

```bash
# Todos estos targets declaran:
Conflicts=shutdown.target
# → Primero detienen todos los servicios
# → Luego ejecutan el shutdown
```

### rescue.target — Modo single-user (mantenimiento)

```bash
# rescue.target:
# → sysinit.target completado (✓)
# → filesystem raíz montado RW (✓)
# → Red DESACTIVADA
# → Solo root shell en consola
# → Servicios de usuario NO Arrancados
# → Para: reparar configuraciones, resetear passwords

sudo systemctl isolate rescue.target
```

### emergency.target — Modo de emergencia

```bash
# emergency.target (más restrictivo que rescue):
# → sysinit.target NO completado (✗)
# → filesystem raíz SOLO READ-ONLY
# → udev NO activo
# → journald NO activo
# → Servicios NO Arrancados
# → Para: boot fs corrompido, /etc/fstab con errores

sudo systemctl isolate emergency.target

# En emergency, si necesitas escribir:
mount -o remount,rw /
# Remonta / como read-write para poder editar
```

```bash
┌─────────────────────────────────────────────────────────────┐
│              rescue vs emergency                                │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Característica          │ rescue    │ emergency            │
│  ────────────────────────┼───────────┼──────────           │
│  sysinit.target         │ ✓ hecho   │ ✗ NO hecho         │
│  Root filesystem        │ RW        │ RO                  │
│  udev                  │ ✓ activo  │ ✗ no activo       │
│  journald               │ ✓ activo  │ ✗ no activo       │
│  Red                   │ ✗         │ ✗                  │
│  Servicios             │ ✗         │ ✗                  │
│                                                              │
│  Cuándo usar           │ Problemas  │ Boot/fs dañados     │
│                        │ de config │ errores en fstab    │
└─────────────────────────────────────────────────────────────┘
```

### multi-user.target — Servidor (sin GUI)

```bash
# multi-user.target:
# → Red configurada (network.target ✓)
# → Todos los servicios del sistema corriendo
# → Login por consola y SSH
# → SIN interfaz gráfica

# Este es el default en servidores:
systemctl get-default
# multi-user.target
```

### graphical.target — Desktop (con GUI)

```bash
# graphical.target:
# → Extiende multi-user.target
# → display manager corriendo (GDM, SDDM, LightDM)
# → Servidor X11/Wayland activo
# → Sesión gráfica disponible

# systemd show que graphical DEPENDE de multi-user:
systemctl list-dependencies graphical.target --no-pager | head -15
# graphical.target
# ● ├─display-manager.service
# ● └─multi-user.target
#   ├─sshd.service
#   ├─cron.service
#   ├─...
```

## Targets de sincronización (boot)

Estos targets no son "estados del sistema" — son **puntos de sincronización** que otros targets usan:

```bash
# LOCAL FS
local-fs.target          → filesystems locales montados (de /etc/fstab)
local-fs-pre.target      → antes de montar fs locales

# EARLY BOOT
sysinit.target          → inicialización temprana (udev, tmpfiles, journal, swap)
basic.target            → sistema base (D-Bus, sockets base, timers)

# NETWORK
network.target         → interfaces con IP (bind() funciona, PERO puede no haber ruta)
network-online.target  → conectividad real verificada (para servicios que conectan a remote)

# PARALLEL UNITS
sockets.target          → todos los sockets de activación escuchando
timers.target           → todos los timers activos

# USER SESSIONS
nss-user-lookup.target  → NSS (getpwnam, etc.)
remote-fs.target        → filesystems remotos montados (NFS, CIFS)
```

### Cadena de boot completa

```
systemd (PID 1)
    │
    ▼
local-fs-pre.target
    │
    ▼
local-fs.target ──── swap.target
    │
    ▼
sysinit.target
    │
    ├─► basic.target ──► (sockets.target ─► timers.target ─► paths.target)
    │
    ▼
network.target ──── network-online.target
    │
    ▼
multi-user.target ──── (graphical.target)
    │
    ▼
    (los servicios想买想买买 se inician via WantedBy=)
```

## network.target vs network-online.target

```
network.target:
  → Las interfaces tienen IP asignada
  → bind() a un puerto funciona
  → PERO la ruta por defecto puede no estar configurada
  → DHCP puede estar Renewing
  → Suficiente para: servidores que ESCUCHAN (nginx, sshd)

network-online.target:
  → Hay conectividad verificada
  → La ruta por defecto está configurada
  → DNS funciona
  → Suficiente para: servicios que CONECTAN a hosts remotos
  → Más lento (espera a que la red esté realmente usable)
```

```bash
# Servicios que ESCUCHAN en la red:
systemctl show nginx.service | grep After
# After=network.target remote-fs.target nss-lookup.target

# Servicios que CONECTAN a la red:
systemctl show postgresql.service | grep After
# After=network.target remote-fs.target
# After=network-online.target (para algunos configs)
```

## Targets personalizados

Se pueden crear targets custom para casos de uso específicos:

```ini
# /etc/systemd/system/myapp.target
[Unit]
Description=My Application Target
After=network-online.target multi-user.target
Wants=myapp.service myapp-worker.service

[Install]
WantedBy=multi-user.target
```

```bash
# Crear el target
sudo systemctl edit --full --runtime myapp.target

# Usarlo
sudo systemctl isolate myapp.target

# O сделать default:
sudo systemctl set-default myapp.target
```

## Dependencias entre targets

```bash
# Ver qué necesita un target:
systemctl list-dependencies multi-user.target --no-pager | head -30

# Ver qué targets necesitan un servicio:
systemctl list-dependencies --reverse multi-user.target --no-pager | head -20
```

## Cambiar entre targets

```bash
# Cambiar target actual (sin reiniciar):
sudo systemctl isolate multi-user.target
sudo systemctl isolate graphical.target

# isolate = cambiar a ese target,
# detieniendo todos los servicios que no sean dependencias

# Establecer target por defecto para siguientes arranques:
sudo systemctl set-default multi-user.target
sudo systemctl set-default graphical.target

# Ver target por defecto actual:
systemctl get-default
```

## Diferencias clave SysV vs systemd

```
┌─────────────────────────────────────────────────────────────┐
│                 SYSVINIT                  │    SYSTEMD              │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Solo UN runlevel activo             │  Múltiples targets activos  │
│  Arranque SECUENCIAL               │  Arranque PARALELO          │
│  Dependencias NUMÉRICAS (S01-S99)  │  Dependencias DECLARATIVAS  │
│  Runlevels FIJOS (0-6)             │  Targets ILIMITADOS          │
│  Agregar servicio = 6 symlinks     │  Agregar = archivo + WantedBy= │
│  Sin gestión de sockets/timers      │  sockets, timers, paths      │
│  No hay reload sin restart          │  reload sin restart          │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1 — Explorar targets activos

```bash
# 1. Target por defecto
systemctl get-default

# 2. Cuántos targets están activos
systemctl list-units --type=target --state=active --no-pager | tail -5

# 3. Ver todos los targets activos con sus deps
systemctl list-units --type=target --state=active --no-pager

# 4. Ver el estado de graphical.target
systemctl status graphical.target 2>/dev/null || echo "not active or not found"
```

### Ejercicio 2 — Aliases de runlevel

```bash
# 1. Verificar que los aliases existen
for i in 0 1 2 3 4 5 6; do
    LINK="/usr/lib/systemd/system/runlevel${i}.target"
    TARGET=$(readlink -f "$LINK" 2>/dev/null)
    echo "runlevel${i}.target → $(basename "$TARGET")"
done

# 2. ¿Qué runlevel estás?
runlevel
```

### Ejercicio 3 — Dependencias de multi-user vs graphical

```bash
# 1. Comparar qué tiene graphical que multi-user no tiene
diff -y --width=80 \
    <(systemctl list-dependencies multi-user.target --plain 2>/dev/null | sort) \
    <(systemctl list-dependencies graphical.target --plain 2>/dev/null | sort) \
    | grep -E "^\w.*<|^\w.*>" | head -20

# 2. Ver el display manager
systemctl status $(systemctl show graphical.target -p WantedBy --value 2>/dev/null) 2>/dev/null || \
    systemctl list-units --type=service --state=active --no-pager | grep -i display

# 3. Ver qué targets necesitan network-online
systemctl list-dependencies --reverse network-online.target --no-pager 2>/dev/null | grep service | head -10
```

### Ejercicio 4 — Cambiar entre targets

```bash
# 1. Ver target actual
systemctl get-default

# 2. Si graphical está activo, cambiar a multi-user
# (En un VM o entorno de prueba)
# sudo systemctl isolate multi-user.target

# 3. Ver qué cambió
# systemctl list-units --type=target --state=active --no-pager | grep target

# 4. Volver a graphical
# sudo systemctl isolate graphical.target

# Nota: isolate solo cambia el target actual,
# no cambia el default para el próximo boot
```

### Ejercicio 5 — rescue y emergency

```bash
# 1. Ver qué pasa con rescue.target
systemctl list-dependencies rescue.target --no-pager | head -20

# 2. Comparar con emergency.target
systemctl list-dependencies emergency.target --no-pager | head -20

# 3. Ver que sysinit.target es diferente
systemctl is-active sysinit.target
```

### Ejercicio 6 — Targets de red

```bash
# 1. ¿Está la red completamente online?
systemctl is-active network-online.target

# 2. Ver servicios que esperan network-online
systemctl list-dependencies --reverse network-online.target --no-pager 2>/dev/null | grep service | head -10

# 3. Ver qué servicios usan solo network.target
systemctl list-dependencies --reverse network.target --no-pager 2>/dev/null | grep service | wc -l
```
