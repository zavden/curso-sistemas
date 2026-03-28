# T03 — Estados de unidad

> **Fuentes:** `README.md` + `README.max.md` + `labs/README.md`

## Erratas corregidas

| # | Ubicación | Error | Corrección |
|---|-----------|-------|------------|
| 1 | max:158,508 | Texto en chino: `"Son必需品 del sistema"` | `"Son esenciales para el sistema"` |
| 2 | max:246 | Texto en chino: `"un servicio危险的 arranque"` | `"un servicio peligroso arranque"` |
| 3 | max:290 | `Result=exit-code-or-signaled → kombinasi` (indonesio) | `→ combinación de exit-code y señal` |
| 4 | max:302 | Propiedades `ExecStartMainExitCode`, `ExecStartMainExitStatus` | No existen; las correctas son `ExecMainCode` y `ExecMainStatus` |
| 5 | max:239 | `mask postfix.service # usar sendmail en su lugar` | Al revés: se maskea sendmail para usar postfix (no viceversa) |
| 6 | max:324 | `systemctl list-dependencies myapp.service --failed` | `--failed` no es un flag válido para `list-dependencies`; usar `systemctl --failed` |

---

## Dos dimensiones de estado

Cada unidad tiene **dos estados independientes** que deben entenderse por
separado:

```
┌─────────────────────────────────────────────────────────┐
│  LOAD STATE — ¿Se pudo leer el archivo de unidad?       │
│                                                         │
│  loaded    → parseado correctamente, en memoria         │
│  not-found → no se encontró el archivo                  │
│  error     → error al parsear                           │
│  masked    → bloqueado con symlink a /dev/null          │
├─────────────────────────────────────────────────────────┤
│  ACTIVE STATE — ¿La unidad está haciendo su trabajo?    │
│                                                         │
│  active    → corriendo/montada/escuchando               │
│  inactive  → no está activa                             │
│  failed    → terminó con error                          │
├─────────────────────────────────────────────────────────┤
│  UNIT FILE STATE — ¿Arrancará al boot?                  │
│                                                         │
│  enabled   → symlink existe en target.wants/            │
│  disabled  → no hay symlink                             │
│  masked    → symlink a /dev/null                        │
│  static    → no tiene [Install], se activa por dep      │
└─────────────────────────────────────────────────────────┘
```

```bash
# Todo junto en systemctl status:
systemctl status nginx.service
#    Loaded: loaded (/usr/lib/systemd/system/nginx.service; enabled; preset: enabled)
#                 ▲                                              ▲        ▲
#            LoadState                                     UnitFileState  Preset
#
#    Active: active (running) since Mon 2024-03-17 10:00:00 UTC; 5h ago
#                 ▲               ▲
#            ActiveState      SubState

# Estados en formato máquina:
systemctl show nginx.service --property=LoadState,ActiveState,SubState,UnitFileState
```

---

## Load states

| LoadState | Significado | ¿Se puede iniciar? |
|---|---|---|
| `loaded` | Parseado OK, en memoria | Sí (si no está masked) |
| `not-found` | No existe archivo | No |
| `bad-setting` | Parseado pero tiene error de config | No |
| `error` | Error al parsear | No |
| `masked` | Enmascarado (symlink a /dev/null) | No |
| `merged` | Fusionado con otra unidad (raro) | Depende |

```bash
systemctl show nginx.service --property=LoadState
# loaded

systemctl show servicio-inexistente.service --property=LoadState
# not-found
```

---

## Active states

| ActiveState | Significado |
|---|---|
| `active` | La unidad está corriendo/montada/escuchando |
| `inactive` | La unidad no está activa |
| `activating` | En proceso de arrancar |
| `deactivating` | En proceso de detenerse |
| `failed` | Terminó con error |
| `reloading` | Recargando su configuración |
| `maintenance` | En mantenimiento (automounts) |

### Sub-states según tipo de unidad

```bash
# Services:
active (running)       # proceso principal corriendo
active (exited)        # ExecStart terminó OK (Type=oneshot con RemainAfterExit=yes)
active (waiting)       # esperando (ej: Type=idle)
inactive (dead)        # no corriendo

# Sockets:
active (listening)     # escuchando conexiones
active (running)       # conexión activa

# Mounts:
active (mounted)       # montado
inactive (dead)        # no montado

# Timers:
active (waiting)       # esperando el próximo disparo
active (elapsed)       # ya se disparó, sin próximo programado

# Paths:
active (waiting)       # monitoreando cambios en filesystem
```

---

## Unit File State — ¿Arrancará al boot?

Independiente del active state, una unidad puede estar **habilitada** o
**deshabilitada** para arrancar en el boot:

| UnitFileState | ¿Arranca al boot? | Significado |
|---|---|---|
| `enabled` | Sí | Symlink en `target.wants/` |
| `disabled` | No | Sin symlink |
| `masked` | No (bloqueado) | Symlink a `/dev/null` |
| `static` | N/A | No tiene `[Install]`, se activa como dependencia |
| `indirect` | Indirectamente | Habilitado vía `Also=` en otra unidad |
| `generated` | Dinámicamente | Creado por un generator |
| `transient` | Runtime | Creado durante esta sesión |
| `enabled-runtime` | Solo esta sesión | No persiste al reiniciar |

```bash
systemctl is-enabled nginx.service
# enabled, disabled, masked, static, etc.

# Ver todas las unidades y su UnitFileState:
systemctl list-unit-files --type=service
# UNIT FILE                    STATE     PRESET
# nginx.service                enabled   enabled
# sshd.service                 enabled   enabled
# bluetooth.service            disabled  enabled
# debug-shell.service          static    -
```

### Servicios static

Los servicios "static" no tienen sección `[Install]`. No se pueden
`enable`/`disable` directamente — siempre se activan como dependencia
de otras unidades:

```bash
# Muchos servicios core son static:
systemctl is-enabled systemd-journald.service    # static
systemctl is-enabled dbus.service                # static
systemctl is-enabled systemd-logind.service      # static

# Son esenciales — siempre los necesita algo, no tiene sentido deshabilitarlos

# Verificar que no tiene [Install]:
systemctl cat systemd-journald.service | grep "\[Install\]"
# (sin output = no tiene sección [Install])
```

### Presets

Los presets definen el estado por defecto de `enable`/`disable` al instalar
un paquete:

```bash
# Ver preset files:
ls /usr/lib/systemd/system-preset/
cat /usr/lib/systemd/system-preset/90-systemd.preset
# enable sshd.service
# disable debug-shell.service

# Aplicar todos los presets:
sudo systemctl preset-all

# Diferencias entre distribuciones:
# Debian: pocos presets, enable/disable manual
# RHEL/Fedora: usa presets activamente
```

---

## Masked — Bloqueo total

Una unidad enmascarada no puede arrancar bajo ninguna circunstancia — ni
manualmente, ni como dependencia, ni al boot:

```
disable vs mask:

  disable:
  - No arranca al boot
  - systemctl start nginx → FUNCIONA
  - Se elimina symlink de target.wants/

  mask:
  - No arranca al boot
  - systemctl start nginx → ERROR (Unit is masked)
  - No puede ser iniciada como dependencia
  - Symlink a /dev/null en /etc/systemd/system/
```

```bash
# Enmascarar:
sudo systemctl mask nginx.service
# Created symlink /etc/systemd/system/nginx.service → /dev/null

# Intentar arrancar:
sudo systemctl start nginx.service
# Failed to start nginx.service: Unit nginx.service is masked.

# Ver estado:
systemctl is-enabled nginx.service
# masked

# Desenmascarar:
sudo systemctl unmask nginx.service
# Removed /etc/systemd/system/nginx.service
```

```bash
# Cuándo enmascarar:
# 1. Prevenir arranque accidental:
sudo systemctl mask iptables.service    # si usas firewalld
sudo systemctl mask sendmail.service    # si usas postfix

# 2. Durante migración de servicios
# 3. Seguridad — desactivar servicios peligrosos:
sudo systemctl mask debug-shell.service

# 4. Cuando dos servicios conflictúan (ej: docker y podman)
```

---

## Failed state y diagnóstico

Cuando un servicio falla (exit code != 0, señal, timeout), entra en estado
`failed`:

```bash
systemctl status myapp.service
# Active: failed (Result: exit-code) since ...
# Process: 12345 ExecStart=... (code=exited, status=1/FAILURE)

# El estado failed persiste hasta que se resetea:
sudo systemctl reset-failed myapp.service
# Vuelve a: inactive (dead)

# Resetear TODAS las unidades fallidas:
sudo systemctl reset-failed
```

### Propiedades de diagnóstico

```bash
# Result: la causa del fallo
systemctl show myapp.service --property=Result
# Result=exit-code        → proceso salió con código != 0
# Result=signal           → recibió señal (SIGSEGV, etc.)
# Result=timeout          → TimeoutStartSec/TimeoutStopSec expiró
# Result=core-dump        → generó core dump
# Result=watchdog         → WatchdogSec expiró
# Result=start-limit-hit  → demasiados reinicios (StartLimitBurst)
# Result=resources        → no se pudieron asignar recursos

# Exit code y señal:
systemctl show myapp.service --property=ExecMainCode,ExecMainStatus
```

### Flujo de diagnóstico completo

```bash
# 1. Ver status con logs recientes:
systemctl status myapp.service -l --no-pager

# 2. Ver logs del journal:
journalctl -u myapp.service --no-pager -n 50
journalctl -u myapp.service -b             # desde el último boot
journalctl -u myapp.service --since "1 hour ago"

# 3. Ver Result y ExitCode:
systemctl show myapp.service --property=Result,ExecMainCode,ExecMainStatus

# 4. Si fue OOM kill, verificar kernel:
dmesg | grep -i "killed process"
journalctl -k --since "1 hour ago" | grep -i oom

# 5. Ver límites de recursos:
systemctl show myapp.service --property=MemoryMax,LimitNOFILE
```

---

## Consultar estados

```bash
# Comandos rápidos (exit code 0 = sí, != 0 = no):
systemctl is-active nginx.service       # ¿está corriendo?
systemctl is-enabled nginx.service      # ¿arranca al boot?
systemctl is-failed nginx.service       # ¿está en failed?

# Con --quiet para scripts (sin output, solo exit code):
if systemctl is-active --quiet nginx.service; then
    echo "nginx está corriendo"
fi

# Listar por estado:
systemctl list-units --state=failed
systemctl list-units --state=active --type=service
systemctl list-unit-files --state=enabled
systemctl list-unit-files --state=masked

# Listar unidades fallidas (shortcut):
systemctl --failed
```

---

## Diagrama de transiciones

```
                  ENABLE/DISABLE (solo afecta UnitFileState)

                    ┌─────────────┐
          enable    │             │  disable
        ┌──────────▶│  Habilitada  │◀──────────┐
        │           │  (boot)     │            │
        │           └─────────────┘            │
        │                                      │
  ┌─────┴──────┐    start    ┌──────────┐    ┌─┴───────────┐
  │  inactive   │───────────▶│  active   │    │  disabled    │
  │  (dead)     │◀───────────│ (running) │    │  (no boot)  │
  └──────▲──────┘    stop    └─────┬─────┘    └─────────────┘
         │                         │
         │         crash/error     │
         │                    ┌────▼─────┐
         │    reset-failed    │  failed   │
         └────────────────────│ (error)   │
                              └──────────┘
```

**enabled/disabled es INDEPENDIENTE de active/inactive.** Un servicio puede
estar en cualquier combinación:
- `enabled + active` → arranca al boot y está corriendo ahora
- `enabled + inactive` → arranca al boot pero ahora está parado
- `disabled + active` → no arranca al boot pero fue iniciado manualmente
- `disabled + inactive` → ni arranca al boot ni está corriendo

---

## Lab — Estados de unidad

### Parte 1 — Load y active states

#### Paso 1.1: Las dos dimensiones

```bash
# Ver los dos estados de un servicio:
systemctl status sshd.service

# En formato máquina:
systemctl show sshd.service --property=LoadState,ActiveState,SubState,UnitFileState
```

Observa:
- `Loaded:` muestra LoadState, ruta del archivo, UnitFileState, y preset
- `Active:` muestra ActiveState y SubState

#### Paso 1.2: Explorar load states

```bash
# loaded:
systemctl show sshd.service --property=LoadState
# loaded

# not-found:
systemctl show noexiste.service --property=LoadState
# not-found

# masked (buscar si hay):
for f in /etc/systemd/system/*.service 2>/dev/null; do
    [[ -L "$f" ]] && [[ "$(readlink "$f")" == "/dev/null" ]] && \
        echo "MASKED: $(basename "$f")"
done
```

#### Paso 1.3: Sub-states por tipo

```bash
# Ver sub-states de servicios activos:
systemctl list-units --type=service --state=active --no-pager | head -10

# Ver sub-states de sockets:
systemctl list-units --type=socket --state=active --no-pager

# Ver sub-states de timers:
systemctl list-units --type=timer --state=active --no-pager
```

Compara: services muestran `running` o `exited`, sockets muestran
`listening`, timers muestran `waiting`.

### Parte 2 — UnitFileState

#### Paso 2.1: Enabled vs disabled

```bash
# Ver symlinks que representan "enabled":
ls -la /etc/systemd/system/multi-user.target.wants/ 2>/dev/null | head -10

# Cada symlink es un servicio enabled para multi-user.target
# systemctl enable crea estos symlinks
# systemctl disable los elimina
```

#### Paso 2.2: Servicios static

```bash
# Buscar servicios sin [Install]:
COUNT=0
for svc in /usr/lib/systemd/system/*.service; do
    [[ -f "$svc" ]] || continue
    if ! grep -q "^\[Install\]" "$svc" 2>/dev/null; then
        echo "  $(basename "$svc") — static"
        ((COUNT++))
        [[ $COUNT -ge 8 ]] && break
    fi
done
```

Son esenciales como dependencia de otras unidades — no tiene sentido
deshabilitarlos.

#### Paso 2.3: Presets

```bash
# Ver archivos de preset:
ls /usr/lib/systemd/system-preset/ 2>/dev/null

# Contenido:
PRESET=$(ls /usr/lib/systemd/system-preset/*.preset 2>/dev/null | head -1)
[[ -n "$PRESET" ]] && head -20 "$PRESET"
```

Formato: `enable sshd.service` o `disable debug-shell.service`.

### Parte 3 — Masked y diagnóstico

#### Paso 3.1: Verificaciones en scripts

```bash
SERVICES=(sshd cron nginx)

for svc in "${SERVICES[@]}"; do
    printf "%-15s " "$svc"

    if systemctl is-active --quiet "$svc" 2>/dev/null; then
        printf "ACTIVO  "
    else
        printf "PARADO  "
    fi

    if systemctl is-enabled --quiet "$svc" 2>/dev/null; then
        echo "enabled"
    else
        echo "disabled/static"
    fi
done
```

El flag `--quiet` suprime la salida, solo retorna exit code.

---

## Ejercicios

### Ejercicio 1 — Inspeccionar todos los estados de un servicio

```bash
systemctl show sshd.service --property=LoadState,ActiveState,SubState,UnitFileState
```

¿Qué valores esperas para cada propiedad?

<details><summary>Predicción</summary>

```
LoadState=loaded
ActiveState=active
SubState=running
UnitFileState=enabled
```

- `loaded` → el archivo fue encontrado y parseado sin errores
- `active` → sshd está corriendo
- `running` → el sub-state de un service activo con proceso en ejecución
- `enabled` → tiene symlink en `multi-user.target.wants/`, arranca al boot

Si sshd no estuviera instalado: `LoadState=not-found`.
Si estuviera parado: `ActiveState=inactive`, `SubState=dead`.
Si hubiera fallado: `ActiveState=failed`.

</details>

### Ejercicio 2 — Contar servicios por UnitFileState

```bash
echo "=== enabled ==="
systemctl list-unit-files --type=service --state=enabled --no-pager | tail -1

echo "=== disabled ==="
systemctl list-unit-files --type=service --state=disabled --no-pager | tail -1

echo "=== static ==="
systemctl list-unit-files --type=service --state=static --no-pager | tail -1

echo "=== masked ==="
systemctl list-unit-files --state=masked --no-pager | tail -1
```

¿Qué categoría tiene más servicios?

<details><summary>Predicción</summary>

Típicamente: **static** es la categoría más numerosa — la mayoría de
servicios internos de systemd no tienen `[Install]` y se activan como
dependencia de otros.

Orden habitual: `static` > `disabled` > `enabled` > `masked`.

- **static:** servicios internos de systemd (systemd-journald, dbus, etc.)
- **disabled:** servicios instalados pero no habilitados (bluetooth, cups, etc.)
- **enabled:** servicios activamente habilitados para el boot (sshd, nginx, etc.)
- **masked:** típicamente 0 en un sistema por defecto

</details>

### Ejercicio 3 — Sub-states por tipo de unidad

```bash
# Services activos:
systemctl list-units --type=service --state=active --plain --no-pager | head -5

# Sockets activos:
systemctl list-units --type=socket --state=active --plain --no-pager | head -5

# Timers activos:
systemctl list-units --type=timer --state=active --plain --no-pager | head -5
```

¿Qué sub-state muestra cada tipo?

<details><summary>Predicción</summary>

- **Services:** `running` (proceso en ejecución) o `exited` (completó con éxito,
  típico de `Type=oneshot` con `RemainAfterExit=yes`)
- **Sockets:** `listening` (esperando conexiones)
- **Timers:** `waiting` (esperando el próximo disparo)

Los sub-states reflejan la naturaleza de cada tipo:
- Un service "hace algo" → `running`
- Un socket "espera algo" → `listening`
- Un timer "espera un momento" → `waiting`

</details>

### Ejercicio 4 — Servicios fallidos

```bash
systemctl --failed --no-pager
```

¿Hay servicios en estado `failed`? Si no hay, ¿cómo provocarías uno para
practicar el diagnóstico?

<details><summary>Predicción</summary>

En un sistema saludable, `systemctl --failed` muestra 0 unidades:
```
  UNIT   LOAD   ACTIVE   SUB   DESCRIPTION
0 loaded units listed.
```

Para provocar un fallo y practicar diagnóstico:
```bash
# Crear un servicio que falle:
cat > /tmp/fail.service << 'EOF'
[Unit]
Description=Failing Service

[Service]
Type=oneshot
ExecStart=/bin/false
EOF

sudo cp /tmp/fail.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl start fail.service    # falla inmediatamente
systemctl status fail.service        # Active: failed (Result: exit-code)
systemctl show fail.service --property=Result,ExecMainCode,ExecMainStatus
# Result=exit-code, ExecMainStatus=1

# Limpiar:
sudo systemctl reset-failed fail.service
sudo rm /etc/systemd/system/fail.service
sudo systemctl daemon-reload
```

</details>

### Ejercicio 5 — is-active en scripts

```bash
SERVICES=(sshd cron systemd-journald noexiste)

for svc in "${SERVICES[@]}"; do
    systemctl is-active --quiet "$svc" 2>/dev/null
    ACTIVE=$?

    systemctl is-enabled --quiet "$svc" 2>/dev/null
    ENABLED=$?

    printf "%-25s active=%-3s enabled=%-3s\n" \
        "$svc" \
        "$([ $ACTIVE -eq 0 ] && echo yes || echo no)" \
        "$([ $ENABLED -eq 0 ] && echo yes || echo no)"
done
```

¿Qué resultado da `noexiste`?

<details><summary>Predicción</summary>

```
sshd                      active=yes enabled=yes
cron                      active=yes enabled=yes
systemd-journald          active=yes enabled=no
noexiste                  active=no  enabled=no
```

Puntos clave:
- `sshd` y `cron`: activos y habilitados (arrancan al boot y están corriendo)
- `systemd-journald`: activo pero `is-enabled` retorna non-zero porque
  es **static** (no tiene `[Install]`). `is-enabled` retorna 0 solo para
  `enabled`; `static` no cuenta como "enabled"
- `noexiste`: ambos retornan non-zero (no existe)

`--quiet` suprime todo output — solo importa el exit code.

</details>

### Ejercicio 6 — Diagrama de combinaciones

Un servicio puede tener cualquier combinación de enabled/disabled y
active/inactive. Identifica en tu sistema un ejemplo de cada:

```bash
# enabled + active:
systemctl list-units --type=service --state=active --no-pager | head -5

# enabled + inactive (posible?):
systemctl list-unit-files --type=service --state=enabled --no-pager | \
    while read -r unit state _; do
        [[ "$state" == "enabled" ]] || continue
        systemctl is-active --quiet "$unit" 2>/dev/null || echo "  $unit (enabled pero inactivo)"
    done 2>/dev/null | head -3
```

<details><summary>Predicción</summary>

**enabled + active:** `sshd.service`, `cron.service` — habilitados para
el boot y corriendo ahora.

**enabled + inactive:** servicios habilitados que no están corriendo
actualmente. Ejemplo común: `firewalld.service` puede estar enabled
pero inactive si aún no se ha alcanzado su target, o servicios que solo
corren una vez al boot (`Type=oneshot`) y ya terminaron sin
`RemainAfterExit=yes`.

**disabled + active:** un servicio iniciado manualmente con `systemctl start`
pero que no arranca al boot. Poco común en producción.

**disabled + inactive:** servicios instalados pero no usados. Ejemplo:
`bluetooth.service` en un servidor sin Bluetooth.

La clave: **enabled/disabled controla el boot**, **active/inactive
controla el momento actual**. Son independientes.

</details>

### Ejercicio 7 — Presets: estado por defecto

```bash
ls /usr/lib/systemd/system-preset/
PRESET=$(ls /usr/lib/systemd/system-preset/*.preset 2>/dev/null | head -1)
[[ -n "$PRESET" ]] && cat "$PRESET" | head -15
```

¿Qué servicios están habilitados por defecto? ¿Cuáles deshabilitados?

<details><summary>Predicción</summary>

El archivo de preset contiene líneas como:
```
enable sshd.service
enable systemd-networkd.service
disable debug-shell.service
```

Significado: cuando se instala un paquete que incluye `sshd.service`,
el preset dice que debe quedar `enabled` automáticamente.

En **RHEL/Fedora**: los presets son activos y definen muchos servicios.
En **Debian**: hay pocos presets; la mayoría de servicios se habilitan
durante la instalación del paquete (`deb-systemd-invoke enable`), no
mediante presets.

`sudo systemctl preset-all` aplica todos los presets de golpe — resetea
todos los servicios a su estado predefinido. Útil para volver a un estado
"limpio" pero peligroso en producción (puede deshabilitar servicios que
habilitaste manualmente).

</details>

### Ejercicio 8 — Diagnóstico de Result

```bash
# Si hay un servicio failed:
systemctl --failed --no-pager

# Para cada uno, ver la causa:
# systemctl show SERVICIO --property=Result,ExecMainCode,ExecMainStatus
```

¿Qué significan los valores de `Result`?

<details><summary>Predicción</summary>

| Result | Significado | Ejemplo |
|--------|-------------|---------|
| `exit-code` | El proceso salió con código != 0 | Bug, config inválida |
| `signal` | Recibió señal (crash) | SIGSEGV, SIGABRT |
| `timeout` | TimeoutStartSec o TimeoutStopSec expiró | Servicio tarda demasiado en arrancar |
| `core-dump` | Generó core dump | Crash con volcado de memoria |
| `watchdog` | WatchdogSec expiró sin ping | El servicio se colgó |
| `start-limit-hit` | StartLimitBurst superado | Demasiados reinicios en poco tiempo |
| `resources` | No se pudieron asignar recursos | Sin memoria, sin file descriptors |

Para cada tipo de fallo, el diagnóstico es diferente:
- `exit-code` → `journalctl -u servicio` para ver mensajes de error
- `signal` → `dmesg` o `coredumpctl` para ver el crash
- `timeout` → aumentar `TimeoutStartSec=` o investigar por qué tarda
- `start-limit-hit` → `systemctl reset-failed` y luego investigar la causa raíz

</details>

### Ejercicio 9 — Flujo de diagnóstico completo

Si `myapp.service` está en `failed`, ¿en qué orden ejecutarías estos
comandos y por qué?

```bash
# a)
journalctl -u myapp.service --no-pager -n 50

# b)
systemctl status myapp.service -l --no-pager

# c)
systemctl show myapp.service --property=Result,ExecMainCode,ExecMainStatus

# d)
sudo systemctl reset-failed myapp.service

# e)
sudo systemctl start myapp.service
```

<details><summary>Predicción</summary>

Orden correcto: **b → c → a → d → e**

1. **b) `systemctl status`** — primer vistazo rápido: muestra el estado,
   el PID, el Result, y las últimas líneas del log. Suficiente para
   diagnosticar problemas simples.

2. **c) `systemctl show --property=Result`** — confirmar la causa exacta
   del fallo (exit-code, signal, timeout, etc.).

3. **a) `journalctl -u`** — si el status no es suficiente, ver el log
   completo del servicio para mensajes de error detallados.

4. **d) `reset-failed`** — limpiar el estado failed para poder reintentar.
   **IMPORTANTE:** hacer DESPUÉS de diagnosticar, no antes (el reset borra
   la información de diagnóstico).

5. **e) `start`** — reintentar después de corregir el problema.

Error común: ejecutar `reset-failed` + `start` sin diagnosticar primero.
El servicio fallará de nuevo por la misma razón.

</details>

### Ejercicio 10 — Script de health check

```bash
#!/usr/bin/env bash
set -euo pipefail

CRITICAL_SERVICES=(sshd systemd-journald systemd-logind)
OPTIONAL_SERVICES=(cron nginx docker)

check() {
    local svc=$1 level=$2
    local active enabled
    active=$(systemctl is-active "$svc" 2>/dev/null || true)
    enabled=$(systemctl is-enabled "$svc" 2>/dev/null || true)

    printf "  %-25s active=%-12s enabled=%-12s" "$svc" "$active" "$enabled"

    if [[ "$active" == "failed" ]]; then
        local result
        result=$(systemctl show "$svc" --property=Result 2>/dev/null | cut -d= -f2)
        echo " [FAILED: $result]"
    elif [[ "$active" != "active" && "$level" == "CRITICAL" ]]; then
        echo " [WARNING: not active]"
    else
        echo ""
    fi
}

echo "=== Critical ==="
for svc in "${CRITICAL_SERVICES[@]}"; do check "$svc" CRITICAL; done

echo "=== Optional ==="
for svc in "${OPTIONAL_SERVICES[@]}"; do check "$svc" OPTIONAL; done
```

¿Qué muestra para `systemd-journald` en la columna `enabled`?

<details><summary>Predicción</summary>

```
=== Critical ===
  sshd                      active=active        enabled=enabled
  systemd-journald          active=active        enabled=static
  systemd-logind            active=active        enabled=static
=== Optional ===
  cron                      active=active        enabled=enabled
  nginx                     active=inactive      enabled=disabled      [WARNING: not active]
  docker                    active=inactive      enabled=disabled
```

`systemd-journald` muestra `enabled=static` porque no tiene sección
`[Install]`. Es un servicio esencial que siempre se activa como dependencia.

El script usa `2>/dev/null || true` para que servicios no instalados
(`not-found`) no causen errores con `set -e`. Los servicios críticos
que no están activos reciben un `[WARNING]`, mientras que los opcionales
inactivos se reportan sin alarma.

</details>
