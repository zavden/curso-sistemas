# T01 — Targets vs runlevels

## Errata sobre los materiales originales

| # | Archivo | Línea | Error | Corrección |
|---|---------|-------|-------|------------|
| 1 | max.md | 67 | Texto en chino "维护负担大" en contenido español | Reemplazado por "La carga de mantenimiento es alta" |
| 2 | max.md | 91-96 | Ejemplo fabricado de `list-units`: `local-fs.target` duplicado, `rescue.target` activo junto a `multi-user.target` (imposible en operación normal), SUB state "active-waiting" inexistente | Corregido con salida realista |
| 3 | max.md | 165-169 | Atribuye `Conflicts=shutdown.target` a los targets poweroff/reboot/halt | En realidad los servicios normales declaran `Conflicts=shutdown.target` vía `DefaultDependencies=yes` |
| 4 | max.md | 295 | sockets→timers→paths representados como secuenciales | Son dependencias paralelas de `basic.target` vía `Wants=` |
| 5 | max.md | 304 | Texto en chino "想买想买买" en contenido español | Reemplazado por texto en español |
| 6 | max.md | 358 | Texto en ruso "сделать" en lugar de español | Reemplazado por "Establecer como" |
| 7 | max.md | 381 | Typo "detieniendo" | Corregido: "deteniendo" |
| 8 | max.md | 452 | `systemctl show graphical.target -p WantedBy` — `WantedBy` es directiva de `[Install]`, no propiedad en runtime | Usar `systemctl cat` o `list-dependencies --reverse` |

---

## Runlevels — El modelo SysVinit (legacy)

En SysVinit, el sistema operaba en **runlevels** numerados del 0 al 6. Cada
runlevel definía un estado fijo del sistema con un conjunto determinado de
servicios:

```
Runlevel 0 — Halt (apagar)
Runlevel 1 — Single user (mantenimiento, sin red, sin login remoto)
Runlevel 2 — Multi-user sin NFS (Debian lo usa como multi-user completo)
Runlevel 3 — Multi-user completo con red, sin GUI (Red Hat: el "normal")
Runlevel 4 — Reservado (no usado por defecto, disponible para custom)
Runlevel 5 — Multi-user con GUI (X11/Wayland, display manager)
Runlevel 6 — Reboot (reiniciar)
```

```bash
# Comandos legacy (todavía funcionan por compatibilidad):
runlevel           # N 5 → anterior=N (none), actual=5
init 3             # cambiar a runlevel 3
telinit 3          # equivalente

# Los servicios se controlaban con scripts en /etc/init.d/
# y symlinks en /etc/rc{0-6}.d/:
ls /etc/rc3.d/
# S10syslog    → Start syslog  (S = Start, 10 = orden)
# S20network   → Start network
# K30sshd      → Kill sshd al salir del runlevel (K = Kill)
```

### Problemas del modelo SysVinit

1. **Solo un runlevel activo** — no se pueden combinar estados ("quiero
   multi-user pero sin cron" → no es posible)
2. **Arranque secuencial** — S10 espera a que termine S09, sin paralelismo →
   boot lento
3. **Dependencias numéricas** — el orden se define por número, no por
   semántica ("sshd necesita network" expresado como "S20 viene después de
   S10" — solo por convención)
4. **Difícil de extender** — agregar un servicio requiere crear symlinks
   manualmente en cada directorio `rc{0-6}.d/`
5. **No reutilizable** — runlevels 2, 3 y 4 son casi idénticos pero se
   duplican; la carga de mantenimiento es alta

---

## Targets — El modelo systemd

Los targets reemplazan los runlevels con un modelo flexible. Un target es una
**unidad que agrupa otras unidades** mediante dependencias:

- **Múltiples targets** pueden estar activos simultáneamente
- El arranque es **paralelo** donde las dependencias lo permiten
- Tienen **dependencias explícitas** (After, Wants, Requires)
- Se pueden **crear targets custom** sin límite

```bash
# Ver los targets activos (hay varios simultáneamente):
systemctl list-units --type=target --state=active --no-pager
# UNIT                   LOAD   ACTIVE SUB    DESCRIPTION
# basic.target          loaded active active Basic System
# local-fs.target       loaded active active Local File Systems
# multi-user.target     loaded active active Multi-User System
# network.target        loaded active active Network
# sockets.target        loaded active active Sockets
# sysinit.target        loaded active active System Initialization
# timers.target         loaded active active Timers
# ...
```

---

## Mapeo runlevels → targets

| Runlevel | Target systemd | Descripción |
|---|---|---|
| 0 | `poweroff.target` | Apagar |
| 1 | `rescue.target` | Single-user / mantenimiento |
| 2, 3, 4 | `multi-user.target` | Multi-user sin GUI |
| 5 | `graphical.target` | Multi-user con GUI |
| 6 | `reboot.target` | Reiniciar |

```bash
# systemd mantiene aliases de compatibilidad:
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
# N 5  (N = runlevel anterior [none], 5 = actual)
```

En systemd los runlevels 2, 3 y 4 son equivalentes (`multi-user.target`). La
distinción SysVinit de "2 = sin red, 3 = con red" desaparece — la red se
gestiona con `network.target` independientemente del target principal.

---

## Targets principales

### poweroff.target, reboot.target y halt.target

```bash
# Targets terminales — transiciones de estado del sistema:
sudo systemctl poweroff     # activa poweroff.target → apaga
sudo systemctl reboot       # activa reboot.target → reinicia
sudo systemctl halt         # detiene el sistema sin apagar el hardware

# Equivalencias legacy:
#   systemctl poweroff  =  shutdown -h now  =  init 0
#   systemctl reboot    =  shutdown -r now  =  init 6

# Mecanismo de shutdown:
# Los servicios normales (con DefaultDependencies=yes) declaran
# automáticamente:
#   Conflicts=shutdown.target
#   Before=shutdown.target
# Cuando shutdown.target se activa (como parte de poweroff/reboot),
# el Conflicts provoca que TODOS los servicios normales se detengan
# antes de completar el apagado/reinicio.
```

### rescue.target — Modo de rescate (single-user)

```bash
# rescue.target:
# - sysinit.target completado (udev, tmpfiles, journal) ✓
# - Filesystem raíz montado read-write ✓
# - Red DESACTIVADA
# - Sin servicios de usuario
# - Pide contraseña de root → shell interactivo
# - Útil para: reparar configuraciones, resetear contraseñas

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
# - sysinit.target NO ejecutado (sin udev, sin journal)
# - Filesystem raíz montado en READ-ONLY
# - Casi nada está corriendo
# - Shell root directo (sin contraseña en algunos casos)

sudo systemctl isolate emergency.target

# Cuándo usar emergency en lugar de rescue:
# - rescue.target no arranca (error en sysinit)
# - Filesystem corrupto que impide el montaje normal
# - Problemas con /etc/fstab

# En emergency, para poder escribir archivos:
mount -o remount,rw /
```

### Comparación rescue vs emergency

| Característica | rescue | emergency |
|---|---|---|
| Filesystem raíz | read-write | read-only |
| sysinit.target | completado | no ejecutado |
| udev | activo | no activo |
| journald | activo | no activo |
| Red | no | no |
| Cuándo usar | Problemas de configuración | Boot/filesystem dañado |

### multi-user.target — Servidor sin GUI

```bash
# El target estándar para servidores:
# - Red configurada (network.target activo)
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
systemctl list-dependencies graphical.target --no-pager
# graphical.target
# ● ├─display-manager.service
# ● └─multi-user.target
# ●   ├─sshd.service
# ●   ├─ ...
```

---

## Targets de sincronización

Estos targets no representan "estados del sistema" sino **puntos de
sincronización** durante el boot. Otros servicios y targets los usan como
referencia temporal:

```bash
# FILESYSTEM LOCAL
local-fs-pre.target      # antes de montar filesystems locales
local-fs.target           # filesystems locales montados (de /etc/fstab)

# EARLY BOOT
sysinit.target            # inicialización temprana (udev, tmpfiles, journal, swap)
basic.target              # sistema base funcional (D-Bus, sockets, timers, paths)

# RED
network.target            # interfaces de red configuradas (IP asignada)
network-online.target     # conectividad real verificada (rutas, DNS)

# ACTIVACIÓN PARALELA (parte de basic.target)
sockets.target            # todos los sockets de activación listos
timers.target             # todos los timers configurados
paths.target              # todos los path watchers activos

# OTROS
remote-fs.target          # filesystems remotos montados (NFS, CIFS)
nss-user-lookup.target    # NSS disponible (getpwnam, etc.)
swap.target               # swap activado
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
   ┌────┼────┐          (paralelos, via Wants= de basic.target)
   │    │    │
sockets timers paths
   │    │    │
   └────┼────┘
        │
network.target ──── network-online.target
        │
multi-user.target
        │
graphical.target  (si es el default)
```

> **Nota**: sockets.target, timers.target y paths.target son dependencias
> **paralelas** de basic.target (activadas via `Wants=`), no secuenciales
> entre sí.

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
# ↑ Wants= NECESARIO porque network-online.target no se activa por defecto
```

| Necesidad | Target | Ejemplo |
|---|---|---|
| Escuchar en un puerto | `network.target` | nginx, sshd, PostgreSQL (local) |
| Conectar a host remoto | `network-online.target` | cliente de API, rsync remoto |

---

## Targets personalizados

Se pueden crear targets custom para agrupar servicios relacionados:

```ini
# /etc/systemd/system/myapp.target
[Unit]
Description=My Application Target
After=network-online.target multi-user.target
Wants=myapp-api.service myapp-worker.service myapp-scheduler.service

[Install]
WantedBy=multi-user.target
```

```bash
# Activar el target:
sudo systemctl daemon-reload
sudo systemctl start myapp.target

# Establecer como default:
sudo systemctl set-default myapp.target
```

---

## Cambiar entre targets

```bash
# Ver target por defecto:
systemctl get-default

# Cambiar target actual sin reiniciar:
sudo systemctl isolate multi-user.target
sudo systemctl isolate graphical.target
# isolate = cambiar a ese target, deteniendo todos los servicios
# que no sean dependencias del nuevo target

# Establecer target por defecto para próximos arranques:
sudo systemctl set-default multi-user.target
sudo systemctl set-default graphical.target
# Esto crea un symlink: /etc/systemd/system/default.target → target elegido

# Verificar:
systemctl get-default
```

---

## Dependencias entre targets

```bash
# Ver qué necesita un target:
systemctl list-dependencies multi-user.target --no-pager | head -30

# Ver qué targets/servicios dependen de un target:
systemctl list-dependencies --reverse multi-user.target --no-pager | head -20

# Ver el archivo del target:
systemctl cat multi-user.target
```

---

## Diferencias clave targets vs runlevels

| Aspecto | Runlevels (SysVinit) | Targets (systemd) |
|---|---|---|
| Activos | Solo uno a la vez | Múltiples simultáneamente |
| Arranque | Secuencial (S01, S02...) | Paralelo (según dependencias) |
| Dependencias | Implícitas por número | Explícitas (After, Wants, etc.) |
| Extensibilidad | Fija (0-6) | Ilimitada (targets custom) |
| Granularidad | Gruesa (todo el sistema) | Fina (por servicio/grupo) |
| Agregar servicio | Crear symlinks en rc{0-6}.d/ | Un archivo + `WantedBy=` |
| Sockets/timers | No hay gestión | sockets.target, timers.target |

---

## Ejercicios

### Ejercicio 1 — Explorar targets activos

```bash
# 1. ¿Cuál es el target por defecto?
systemctl get-default

# 2. ¿Cuántos targets están activos ahora?
systemctl list-units --type=target --state=active --no-pager --no-legend | wc -l

# 3. Listar los targets activos:
systemctl list-units --type=target --state=active --no-pager
```

<details><summary>Predicción</summary>

- El default será `graphical.target` (desktop) o `multi-user.target` (servidor)
- Habrá **muchos** targets activos simultáneamente (entre 10 y 20), no solo
  uno — esta es la diferencia clave con runlevels
- En la lista aparecerán: `basic.target`, `local-fs.target`, `sysinit.target`,
  `sockets.target`, `timers.target`, `network.target`, y el target principal

</details>

### Ejercicio 2 — Verificar aliases de runlevel

```bash
for i in {0..6}; do
    TARGET=$(readlink -f /usr/lib/systemd/system/runlevel${i}.target 2>/dev/null)
    echo "Runlevel $i → $(basename "$TARGET" 2>/dev/null || echo 'N/A')"
done
```

<details><summary>Predicción</summary>

```
Runlevel 0 → poweroff.target
Runlevel 1 → rescue.target
Runlevel 2 → multi-user.target
Runlevel 3 → multi-user.target
Runlevel 4 → multi-user.target
Runlevel 5 → graphical.target
Runlevel 6 → reboot.target
```

Los runlevels 2, 3 y 4 apuntan **todos** a `multi-user.target`. La distinción
de SysVinit (2=sin red, 3=con red) desaparece porque la red se maneja
independientemente via `network.target`.

</details>

### Ejercicio 3 — Comparar multi-user.target vs graphical.target

```bash
# ¿Qué servicios adicionales tiene graphical.target sobre multi-user.target?
diff <(systemctl list-dependencies multi-user.target --plain --no-pager | sort) \
     <(systemctl list-dependencies graphical.target --plain --no-pager | sort)
```

<details><summary>Predicción</summary>

- `graphical.target` incluirá **todo** lo de `multi-user.target` más el
  display manager (`display-manager.service` → GDM, SDDM o LightDM)
- Las líneas con `>` (solo en graphical) serán pocas: el display manager y
  quizás algún servicio de sesión gráfica
- Las líneas con `<` (solo en multi-user) no deberían existir porque
  graphical es un **superconjunto** de multi-user

</details>

### Ejercicio 4 — Inspeccionar rescue.target y emergency.target

```bash
# 1. Ver dependencias de rescue.target:
systemctl list-dependencies rescue.target --no-pager

# 2. Ver dependencias de emergency.target:
systemctl list-dependencies emergency.target --no-pager

# 3. Comparar cuántas dependencias tiene cada uno:
echo "rescue: $(systemctl list-dependencies rescue.target --plain --no-pager | wc -l) unidades"
echo "emergency: $(systemctl list-dependencies emergency.target --plain --no-pager | wc -l) unidades"
```

<details><summary>Predicción</summary>

- `rescue.target` tendrá significativamente más dependencias porque incluye
  `sysinit.target` completo (udev, journal, tmpfiles, etc.)
- `emergency.target` tendrá muy pocas dependencias — solo lo mínimo para
  obtener un shell (quizás 2-5 unidades)
- La diferencia en cantidad refleja por qué emergency es el último recurso:
  arranca con casi nada

</details>

### Ejercicio 5 — Archivo de un target

```bash
# 1. Ver el contenido del archivo multi-user.target:
systemctl cat multi-user.target

# 2. Identificar las directivas clave:
systemctl show multi-user.target -p Requires -p Wants -p After -p Conflicts --no-pager
```

<details><summary>Predicción</summary>

- `systemctl cat` mostrará `[Unit]` con `Requires=basic.target`,
  `After=basic.target`, y `Conflicts=rescue.service rescue.target`
- Tendrá `AllowIsolate=yes` (necesario para poder hacer `systemctl isolate`)
- `show` con `-p Wants` mostrará todos los servicios que tienen
  `WantedBy=multi-user.target` en su `[Install]` — será una lista larga
  porque muchos servicios se habilitan contra este target

</details>

### Ejercicio 6 — network.target vs network-online.target

```bash
# 1. ¿Está la red completamente online?
systemctl is-active network-online.target

# 2. Ver qué servicios esperan network-online.target:
systemctl list-dependencies --reverse network-online.target --no-pager 2>/dev/null | head -15

# 3. ¿Cuántos servicios dependen de network.target (más común)?
systemctl list-dependencies --reverse network.target --plain --no-pager 2>/dev/null | wc -l
```

<details><summary>Predicción</summary>

- `network-online.target` estará `active` si la red está funcional
- Pocos servicios dependerán de `network-online.target` — solo los que
  necesitan conectar a hosts remotos al arrancar
- Más servicios dependerán de `network.target` porque es el requisito
  más común (escuchar en un puerto)
- Esto confirma la regla: `network.target` para listeners,
  `network-online.target` para connectors

</details>

### Ejercicio 7 — Cadena de boot real

```bash
# Ver el orden de arranque real del sistema:
systemd-analyze critical-chain --no-pager 2>/dev/null | head -25

# Ver tiempos de boot:
systemd-analyze blame --no-pager 2>/dev/null | head -10
```

<details><summary>Predicción</summary>

- `critical-chain` mostrará la cadena: `graphical.target` (o
  `multi-user.target`) ← `network.target` ← `basic.target` ←
  `sysinit.target` ← `local-fs.target`, con tiempos acumulados
- `blame` mostrará los servicios más lentos primero — típicamente
  `NetworkManager-wait-online.service` (si existe) o servicios de
  inicialización de hardware estarán entre los más lentos
- Ambos comandos confirman que el boot es paralelo: múltiples servicios
  arrancan al mismo tiempo, y la cadena crítica muestra solo el camino
  más lento

</details>

### Ejercicio 8 — Simular isolate (leer sin ejecutar)

```bash
# NO ejecutar en producción — solo entender qué haría:
# Si el default es graphical.target:

# 1. ¿Qué servicios se detendrían al cambiar a multi-user?
# Los que solo existen en graphical pero no en multi-user:
diff <(systemctl list-dependencies multi-user.target --plain --no-pager | sort) \
     <(systemctl list-dependencies graphical.target --plain --no-pager | sort) \
     | grep "^>" | sed 's/^> //'

# 2. Ver qué targets tienen AllowIsolate=yes:
for t in $(systemctl list-unit-files --type=target --no-pager --no-legend | awk '{print $1}'); do
    ISOLATE=$(systemctl show "$t" -p AllowIsolate --value 2>/dev/null)
    [[ "$ISOLATE" == "yes" ]] && echo "$t"
done
```

<details><summary>Predicción</summary>

- Al hacer isolate de `multi-user.target`, se detendrían el display manager
  y servicios exclusivos de la sesión gráfica
- Solo unos pocos targets permiten `AllowIsolate=yes`: los targets
  principales (rescue, emergency, multi-user, graphical, poweroff, reboot)
  y los aliases runlevel{0-6}
- La mayoría de targets de sincronización (basic, sysinit, network) **no**
  permiten isolate porque no representan estados completos del sistema

</details>

### Ejercicio 9 — Target personalizado

```bash
# Crear un target custom que agrupe servicios:
cat << 'EOF'
# /etc/systemd/system/demo.target
[Unit]
Description=Demo Application Target
After=network-online.target
Wants=network-online.target

[Install]
WantedBy=multi-user.target
EOF

# Para que un servicio pertenezca a este target, agregaría en su [Install]:
#   WantedBy=demo.target

# Verificar la sintaxis (sin crear el archivo):
echo "Para crear: sudo tee /etc/systemd/system/demo.target"
echo "Luego:      sudo systemctl daemon-reload"
echo "Activar:    sudo systemctl enable --now demo.target"
echo "Verificar:  systemctl list-dependencies demo.target"
```

<details><summary>Predicción</summary>

- Un target es simplemente un archivo `.target` con sección `[Unit]` y
  opcionalmente `[Install]`
- No tiene sección `[Service]` porque no ejecuta nada por sí mismo — solo
  agrupa
- Los servicios se "agregan" al target poniendo `WantedBy=demo.target` en
  su `[Install]` y haciendo `systemctl enable`
- `list-dependencies` del target mostraría los servicios enlazados

</details>

### Ejercicio 10 — Diagnóstico: ¿por qué un servicio no arrancó?

Un servicio configurado con `After=network-online.target` pero **sin**
`Wants=network-online.target` podría no arrancar correctamente. ¿Por qué?

```bash
# Simular la investigación:
# 1. Ver si network-online.target está activo:
systemctl is-active network-online.target

# 2. Ver quién activa network-online.target:
systemctl list-dependencies --reverse network-online.target --no-pager 2>/dev/null | head -10

# 3. Ver si nuestro servicio hipotético lo necesita:
echo "After= solo establece ORDEN, no ACTIVACIÓN"
echo "Sin Wants= o Requires=, network-online.target podría no activarse"
echo "Y After= sin activación = esperar algo que nunca llega"
```

<details><summary>Predicción</summary>

- `After=network-online.target` solo dice "si network-online.target se
  activa, esperar a que termine antes de arrancar mi servicio"
- **Pero** no garantiza que `network-online.target` se active
- Si ninguna otra unidad declara `Wants=network-online.target`, este target
  podría no activarse, y el servicio arrancaría sin red real
- La solución es agregar `Wants=network-online.target` junto al `After=` —
  recordar los dos ejes: **activación** (Wants/Requires) y **ordenamiento**
  (After/Before)
- Esto conecta con T03 (Dependencias): siempre pensar en ambos ejes

</details>
