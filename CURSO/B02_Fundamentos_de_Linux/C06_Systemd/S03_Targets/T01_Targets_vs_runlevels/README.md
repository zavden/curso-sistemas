# T01 — Targets vs runlevels

## Runlevels — El modelo SysVinit

En SysVinit, el sistema operaba en **runlevels** numerados del 0 al 6. Cada
runlevel definía un estado del sistema con un conjunto fijo de servicios:

```
Runlevel 0 — Halt (apagar)
Runlevel 1 — Single user (mantenimiento, sin red, sin login remoto)
Runlevel 2 — Multi-user sin NFS (Debian lo usa como multi-user completo)
Runlevel 3 — Multi-user completo con red (sin GUI)
Runlevel 4 — No usado (reservado)
Runlevel 5 — Multi-user con GUI (X11/Wayland)
Runlevel 6 — Reboot
```

```bash
# En SysVinit:
runlevel          # ver runlevel actual
init 3            # cambiar a runlevel 3
telinit 3         # equivalente

# Los servicios se controlaban con scripts en /etc/init.d/
# y symlinks en /etc/rc{0-6}.d/
# S20sshd → arrancar sshd (S=Start, 20=orden)
# K80sshd → matar sshd al salir del runlevel (K=Kill)
```

### Problemas del modelo

1. **Solo un runlevel activo** — no se pueden combinar estados
2. **Arranque secuencial** — S10 espera a que termine S09, sin paralelismo
3. **Sin dependencias declarativas** — el orden es numérico, no semántico
4. **Difícil de extender** — agregar un servicio requiere crear symlinks manualmente
5. **No reutilizable** — los runlevels 2/3/4 son casi idénticos pero se duplican

## Targets — El modelo systemd

Los targets reemplazan los runlevels con un modelo flexible:

- Un target es una **unidad que agrupa otras unidades**
- Los targets tienen **dependencias explícitas** entre sí
- **Múltiples targets** pueden estar activos simultáneamente
- El arranque es **paralelo** donde las dependencias lo permiten
- Se pueden **crear targets custom** sin límite

```bash
# Ver los targets activos (hay varios simultáneamente):
systemctl list-units --type=target --state=active --no-pager
# basic.target           active Basic System
# local-fs.target        active Local File Systems
# multi-user.target      active Multi-User System
# network.target         active Network
# sockets.target         active Sockets
# timers.target          active Timers
# ...
```

## Mapeo runlevels → targets

| Runlevel | Target systemd | Descripción |
|---|---|---|
| 0 | `poweroff.target` | Apagar |
| 1 | `rescue.target` | Single-user / mantenimiento |
| 2, 3, 4 | `multi-user.target` | Multi-user sin GUI |
| 5 | `graphical.target` | Multi-user con GUI |
| 6 | `reboot.target` | Reiniciar |

```bash
# systemd mantiene aliases para compatibilidad:
ls -la /usr/lib/systemd/system/runlevel*.target
# runlevel0.target → poweroff.target
# runlevel1.target → rescue.target
# runlevel2.target → multi-user.target
# runlevel3.target → multi-user.target
# runlevel4.target → multi-user.target
# runlevel5.target → graphical.target
# runlevel6.target → reboot.target

# El comando legacy sigue funcionando:
runlevel
# N 5  (N = runlevel anterior, 5 = actual)
```

En systemd los runlevels 2, 3 y 4 son equivalentes (multi-user.target). La
distinción SysVinit de "2 = sin red, 3 = con red" desaparece — la red se
gestiona con network.target independientemente del target principal.

## Targets principales

### poweroff.target y reboot.target

```bash
# Targets terminales — transiciones de estado del sistema:
sudo systemctl poweroff     # activa poweroff.target → apaga
sudo systemctl reboot       # activa reboot.target → reinicia
sudo systemctl halt         # detiene sin apagar el hardware

# Estos targets declaran Conflicts= con todos los servicios normales
# → primero se detiene todo, luego se apaga/reinicia
```

### rescue.target — Modo de rescate

```bash
# Single-user mode:
# - sysinit.target completado (udev, tmpfiles, journal)
# - Filesystem raíz montado read-write
# - Sin red
# - Sin servicios de usuario
# - Pide contraseña de root → shell interactivo
# - Útil para reparar configuraciones, resetear contraseñas

sudo systemctl isolate rescue.target

systemctl list-dependencies rescue.target --no-pager
# rescue.target
# ● ├─rescue.service
# ● ├─system.slice
# ● └─sysinit.target
# ●   ├─ ...
```

### emergency.target — Modo de emergencia

```bash
# Más restrictivo que rescue:
# - Filesystem raíz montado en READ-ONLY
# - sysinit.target NO ejecutado (sin udev, sin journal)
# - Casi nada está corriendo
# - Shell root directo

sudo systemctl isolate emergency.target

# Cuándo usar emergency en lugar de rescue:
# - rescue.target no arranca (error en sysinit)
# - Filesystem corrupto que impide el montaje normal
# - Problemas con /etc/fstab
# Para trabajar en emergency, remontar / en rw:
mount -o remount,rw /
```

| Característica | rescue | emergency |
|---|---|---|
| Filesystem raíz | read-write | read-only |
| sysinit | completado | no ejecutado |
| udev | activo | no activo |
| journald | activo | no activo |
| Red | no | no |
| Cuándo usar | Problemas de config | Problemas de boot/fs |

### multi-user.target — Servidor sin GUI

```bash
# El target estándar para servidores:
# - Red configurada
# - Todos los servicios del sistema arrancados
# - Login por consola y SSH
# - Sin interfaz gráfica

# La mayoría de servidores Linux usan este target como default:
systemctl get-default
# multi-user.target
```

### graphical.target — Desktop con GUI

```bash
# Extiende multi-user.target agregando:
# - Display manager (GDM, SDDM, LightDM)
# - Servidor gráfico (X11/Wayland)

# graphical.target DEPENDE de multi-user.target:
systemctl list-dependencies graphical.target
# graphical.target
# ● ├─display-manager.service
# ● └─multi-user.target
# ●   ├─sshd.service
# ●   ├─ ...
```

## Targets de sincronización

Estos targets no representan "estados del sistema" sino puntos de
sincronización durante el boot:

```bash
# local-fs.target      — filesystems locales montados (de /etc/fstab)
# remote-fs.target     — filesystems remotos montados (NFS, CIFS)
# sysinit.target       — inicialización temprana (udev, tmpfiles, journal, swap)
# basic.target         — sistema base funcional (D-Bus, sockets base, timers base)
# network.target       — interfaces de red configuradas (IP asignada)
# network-online.target — conectividad real verificada (rutas, DNS)
# sockets.target       — todos los sockets de activación listos
# timers.target        — todos los timers configurados
```

### Cadena de boot

```
local-fs-pre.target
        │
local-fs.target ─── swap.target
        │
   sysinit.target
        │
   basic.target
        │
  ┌─────┼──────────────┐
  │     │              │
sockets.target  timers.target  paths.target
  │     │              │
  └─────┼──────────────┘
        │
network.target ──── network-online.target
        │
multi-user.target
        │
graphical.target  (si es el default)
```

### network.target vs network-online.target

```bash
# network.target — interfaces configuradas (IP asignada)
# PERO puede no haber conectividad real (DHCP en progreso, ruta no establecida)
# Suficiente para servicios que ESCUCHAN (bind a una IP/puerto)
After=network.target

# network-online.target — conectividad real verificada
# NetworkManager-wait-online o systemd-networkd-wait-online comprueba la red
# Necesario para servicios que CONECTAN a hosts remotos al arrancar
After=network-online.target
Wants=network-online.target
# Wants= necesario porque network-online.target no se activa por defecto
```

## Diferencias clave targets vs runlevels

| Aspecto | Runlevels | Targets |
|---|---|---|
| Activos | Solo uno a la vez | Múltiples simultáneamente |
| Arranque | Secuencial (S01, S02...) | Paralelo (según dependencias) |
| Dependencias | Implícitas por número | Explícitas (After, Wants, etc.) |
| Extensibilidad | Fija (0-6) | Ilimitada (targets custom) |
| Granularidad | Gruesa (todo el sistema) | Fina (por servicio/grupo) |

---

## Ejercicios

### Ejercicio 1 — Explorar targets

```bash
# 1. ¿Cuál es el target por defecto?
systemctl get-default

# 2. ¿Cuántos targets están activos ahora?
systemctl list-units --type=target --state=active --no-pager --no-legend | wc -l

# 3. Listar los targets activos:
systemctl list-units --type=target --state=active --no-pager
```

### Ejercicio 2 — Verificar los aliases de runlevel

```bash
for i in {0..6}; do
    TARGET=$(readlink -f /usr/lib/systemd/system/runlevel${i}.target 2>/dev/null)
    echo "Runlevel $i → $(basename "$TARGET" 2>/dev/null || echo 'N/A')"
done
```

### Ejercicio 3 — Comparar targets

```bash
# ¿Qué servicios adicionales tiene graphical.target sobre multi-user.target?
diff <(systemctl list-dependencies multi-user.target --plain --no-pager | sort) \
     <(systemctl list-dependencies graphical.target --plain --no-pager | sort)
```
