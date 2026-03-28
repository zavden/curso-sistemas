# Lab — Estados de unidad

## Objetivo

Entender las dos dimensiones de estado de una unidad systemd:
load state (si el archivo fue leido) y active state (si esta
corriendo). Explorar los UnitFileState (enabled, disabled, static,
masked), presets, el flujo de transiciones entre estados, y las
tecnicas de diagnostico de fallos.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Load y active states

### Objetivo

Explorar los estados de carga y activacion de las unidades.

### Paso 1.1: Las dos dimensiones de estado

```bash
docker compose exec debian-dev bash -c '
echo "=== Dos dimensiones de estado ==="
echo ""
echo "Cada unidad tiene dos estados independientes:"
echo ""
echo "1. Load state  — si el archivo fue leido y parseado"
echo "   loaded, not-found, bad-setting, error, masked"
echo ""
echo "2. Active state — si la unidad esta corriendo"
echo "   active, inactive, activating, deactivating, failed"
echo ""
echo "--- En systemctl status se muestra asi ---"
echo "  Loaded: loaded (/usr/lib/.../sshd.service; enabled; preset: enabled)"
echo "  Active: active (running) since Mon 2024-03-17 10:00:00 UTC"
echo "          ▲                     ▲"
echo "          active state          sub-state (especifico del tipo)"
echo ""
echo "--- Sub-states comunes ---"
echo "  Services: running, exited, dead, waiting"
echo "  Sockets:  listening, running"
echo "  Mounts:   mounted, dead"
echo "  Timers:   waiting, elapsed"
'
```

### Paso 1.2: Explorar load states

```bash
docker compose exec debian-dev bash -c '
echo "=== Load states ==="
echo ""
echo "--- loaded: archivo parseado correctamente ---"
echo "La mayoria de las unidades instaladas estan en este estado"
echo ""
echo "--- not-found: archivo no encontrado ---"
echo "Ocurre cuando se referencia una unidad que no existe"
echo ""
echo "--- masked: unidad enmascarada ---"
echo "Symlink a /dev/null — no puede arrancar bajo ninguna"
echo "circunstancia"
echo ""
echo "--- Buscar unidades por su LoadState ---"
echo "Las unidades loaded se encuentran en los directorios de systemd:"
LOADED=$(find /usr/lib/systemd/system /etc/systemd/system \
    -maxdepth 1 -name "*.service" 2>/dev/null | wc -l)
echo "Total .service encontrados: $LOADED"
echo ""
echo "--- Buscar unidades masked ---"
MASKED=0
for f in /etc/systemd/system/*.service 2>/dev/null; do
    [[ -L "$f" ]] && [[ "$(readlink "$f")" == "/dev/null" ]] && {
        echo "  MASKED: $(basename "$f")"
        ((MASKED++))
    }
done
echo "Total masked: $MASKED"
'
```

### Paso 1.3: Active states y sub-states

```bash
docker compose exec debian-dev bash -c '
echo "=== Active states ==="
echo ""
echo "| Estado       | Significado                           |"
echo "|------------- |---------------------------------------|"
echo "| active       | Corriendo/montada/escuchando          |"
echo "| inactive     | No activa                             |"
echo "| activating   | En proceso de arrancar                |"
echo "| deactivating | En proceso de detenerse               |"
echo "| failed       | Fallo (exit code != 0, senal, timeout)|"
echo ""
echo "--- Sub-states por tipo ---"
echo ""
echo "Services:"
echo "  active (running)  — proceso corriendo"
echo "  active (exited)   — ExecStart termino OK (Type=oneshot)"
echo "  inactive (dead)   — no corriendo"
echo ""
echo "Sockets:"
echo "  active (listening) — esperando conexiones"
echo "  active (running)   — conexion activa"
echo ""
echo "Timers:"
echo "  active (waiting) — esperando proximo disparo"
echo "  active (elapsed) — ya se disparo, sin proximo programado"
'
```

---

## Parte 2 — UnitFileState

### Objetivo

Explorar los estados de habilitacion de unidades: enabled, disabled,
static, y masked.

### Paso 2.1: Enabled vs disabled

```bash
docker compose exec debian-dev bash -c '
echo "=== Enabled vs disabled ==="
echo ""
echo "enabled  — arranca automaticamente al boot"
echo "           (symlink en target.wants/)"
echo "disabled — NO arranca al boot"
echo "           (sin symlink, pero se puede start manualmente)"
echo ""
echo "--- Symlinks que representan enabled ---"
ls -la /etc/systemd/system/multi-user.target.wants/ 2>/dev/null | head -10
echo ""
echo "Cada symlink es un servicio enabled para multi-user.target"
echo "systemctl enable crea estos symlinks"
echo "systemctl disable los elimina"
'
```

### Paso 2.2: Static — sin [Install]

Antes de ejecutar, predice: por que un servicio seria "static"
en lugar de enabled/disabled?

```bash
docker compose exec debian-dev bash -c '
echo "=== Unidades static ==="
echo ""
echo "Una unidad es static cuando NO tiene seccion [Install]"
echo "No se puede enable/disable directamente"
echo "Solo se activa como dependencia de otra unidad"
echo ""
echo "--- Ejemplos de unidades static ---"
COUNT=0
for svc in /usr/lib/systemd/system/*.service; do
    [[ -f "$svc" ]] || continue
    if ! grep -q "^\[Install\]" "$svc" 2>/dev/null; then
        echo "  $(basename "$svc")"
        ((COUNT++))
        [[ $COUNT -ge 8 ]] && break
    fi
done
echo ""
echo "Total sin [Install]: estas unidades son siempre necesarias"
echo "como dependencia de otras, o son servicios internos de systemd"
echo "que no tiene sentido deshabilitar"
'
```

### Paso 2.3: Presets

```bash
docker compose exec debian-dev bash -c '
echo "=== Presets ==="
echo ""
echo "Los presets definen el estado default de enable/disable"
echo "al instalar un paquete"
echo ""
echo "--- Archivos de preset ---"
ls /usr/lib/systemd/system-preset/ 2>/dev/null
echo ""
echo "--- Contenido de un preset ---"
PRESET=$(ls /usr/lib/systemd/system-preset/*.preset 2>/dev/null | head -1)
if [[ -n "$PRESET" ]]; then
    echo "Archivo: $PRESET"
    head -20 "$PRESET"
    echo "..."
fi
echo ""
echo "Formato:"
echo "  enable sshd.service     ← habilitado por defecto"
echo "  disable debug-shell.service ← deshabilitado por defecto"
echo ""
echo "Diferencia por distro:"
echo "  Debian: pocos presets, enable/disable manual"
echo "  RHEL/Fedora: usa presets activamente"
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== Presets en AlmaLinux ==="
echo ""
ls /usr/lib/systemd/system-preset/ 2>/dev/null
echo ""
PRESET=$(ls /usr/lib/systemd/system-preset/*.preset 2>/dev/null | head -1)
if [[ -n "$PRESET" ]]; then
    echo "Archivo: $PRESET"
    head -15 "$PRESET"
    echo "..."
fi
'
```

---

## Parte 3 — Masked y diagnostico

### Objetivo

Entender el enmascaramiento de unidades y las tecnicas de
diagnostico cuando un servicio falla.

### Paso 3.1: Masked — bloqueo total

```bash
docker compose exec debian-dev bash -c '
echo "=== Masked vs disabled ==="
echo ""
echo "--- disabled ---"
echo "No arranca al boot"
echo "PERO se puede iniciar manualmente con systemctl start"
echo ""
echo "--- masked ---"
echo "No se puede iniciar bajo NINGUNA circunstancia"
echo "Ni manualmente, ni como dependencia, ni al boot"
echo ""
echo "--- Como funciona internamente ---"
echo "systemctl mask nginx.service crea:"
echo "  /etc/systemd/system/nginx.service → /dev/null"
echo ""
echo "Al intentar arrancar:"
echo "  Failed to start nginx.service: Unit nginx.service is masked."
echo ""
echo "--- Cuando enmascarar ---"
echo "  1. Prevenir arranque accidental"
echo "     (ej: mask iptables si usas firewalld)"
echo "  2. Durante migracion de servicios"
echo "  3. Seguridad — desactivar servicios peligrosos"
echo "     (ej: mask debug-shell.service)"
echo ""
echo "--- Verificar si hay unidades masked ---"
for f in /etc/systemd/system/*.service 2>/dev/null; do
    [[ -L "$f" ]] && [[ "$(readlink "$f")" == "/dev/null" ]] && \
        echo "  MASKED: $(basename "$f")"
done
echo "(si no hay salida, ninguna unidad esta masked)"
'
```

### Paso 3.2: Diagnostico de fallos

```bash
docker compose exec debian-dev bash -c '
echo "=== Diagnostico de fallos ==="
echo ""
echo "Cuando un servicio falla, entra en estado failed"
echo "El estado persiste hasta que se resetea"
echo ""
echo "--- Flujo de diagnostico ---"
echo "1. systemctl status myapp.service -l --no-pager"
echo "   → Ver estado, PID, ultimas lineas de log"
echo ""
echo "2. journalctl -u myapp.service --no-pager -n 50"
echo "   → Ver los logs del servicio"
echo ""
echo "3. systemctl show myapp.service --property=Result"
echo "   → Causa del fallo:"
echo "     exit-code    = proceso salio con error"
echo "     signal       = proceso recibio senal (crash)"
echo "     timeout      = TimeoutStartSec/TimeoutStopSec expiro"
echo "     core-dump    = proceso genero core dump"
echo "     watchdog     = WatchdogSec expiro"
echo "     start-limit-hit = demasiados reinicios"
echo ""
echo "4. systemctl reset-failed myapp.service"
echo "   → Limpiar el estado failed para poder reiniciar"
'
```

### Paso 3.3: Diagrama de transiciones

```bash
docker compose exec debian-dev bash -c '
echo "=== Diagrama de transiciones ==="
echo ""
echo "                    ┌─────────────┐"
echo "          enable    │             │  disable"
echo "        ┌──────────▶│  Habilitada  │◀──────────┐"
echo "        │           │  (boot)     │            │"
echo "        │           └─────────────┘            │"
echo "        │                                      │"
echo "  ┌─────┴──────┐    start    ┌──────────┐    ┌─┴───────────┐"
echo "  │  inactive   │───────────▶│  active   │    │  disabled    │"
echo "  │  (dead)     │◀───────────│ (running) │    │  (no boot)  │"
echo "  └──────▲──────┘    stop    └─────┬─────┘    └─────────────┘"
echo "         │                         │"
echo "         │         crash/error     │"
echo "         │                    ┌────▼─────┐"
echo "         │    reset-failed    │  failed   │"
echo "         └────────────────────│ (error)   │"
echo "                              └──────────┘"
echo ""
echo "enabled/disabled es INDEPENDIENTE de active/inactive"
echo "Un servicio puede estar:"
echo "  enabled + active   = arranca al boot y esta corriendo"
echo "  enabled + inactive = arranca al boot pero ahora esta parado"
echo "  disabled + active  = no arranca al boot pero fue iniciado manual"
echo "  disabled + inactive = ni arranca al boot ni esta corriendo"
'
```

### Paso 3.4: Verificaciones en scripts

```bash
docker compose exec debian-dev bash -c '
echo "=== Verificaciones en scripts ==="
echo ""
echo "Comandos rapidos (exit code 0 = si, != 0 = no):"
echo ""
echo "  systemctl is-active SERVICE   → esta corriendo?"
echo "  systemctl is-enabled SERVICE  → arranca al boot?"
echo "  systemctl is-failed SERVICE   → esta en estado failed?"
echo ""
echo "--- Patron en scripts ---"
cat << '\''EOF'\''
#!/usr/bin/env bash
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
        echo "disabled"
    fi
done
EOF
echo ""
echo "El flag --quiet suprime la salida, solo retorna exit code"
echo "Util para condicionales en scripts"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Cada unidad tiene dos estados independientes: **load state**
   (si el archivo fue leido: loaded, not-found, masked) y
   **active state** (si esta corriendo: active, inactive, failed).

2. Los sub-states dan detalle segun el tipo: `running` y `exited`
   para services, `listening` para sockets, `waiting` para timers.

3. **UnitFileState** es independiente del active state: `enabled`
   (arranca al boot), `disabled` (no arranca), `static` (sin
   [Install], solo como dependencia), `masked` (bloqueada).

4. **Masked** es mas fuerte que disabled: crea un symlink a
   `/dev/null` que impide el arranque bajo cualquier circunstancia.
   Usar para prevenir arranques accidentales o por seguridad.

5. Los **presets** definen el estado default de enable/disable al
   instalar un paquete. RHEL los usa activamente; Debian menos.

6. El estado **failed** persiste hasta `reset-failed`. La propiedad
   `Result` indica la causa: exit-code, signal, timeout, watchdog,
   start-limit-hit.
