# Lab — systemctl

## Objetivo

Dominar systemctl como herramienta central de gestion de systemd:
inspeccionar servicios con status/show/cat, listar y filtrar
unidades por tipo y estado, entender enable/disable vs mask/unmask,
y usar verificaciones silenciosas en scripts.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Inspeccion de servicios

### Objetivo

Usar systemctl para inspeccionar el estado y configuracion de
servicios instalados.

### Paso 1.1: systemctl status

```bash
docker compose exec debian-dev bash -c '
echo "=== systemctl status ==="
echo ""
echo "status muestra informacion completa de un servicio:"
echo ""
echo "Ejemplo de salida tipica:"
echo "● nginx.service - A high performance web server"
echo "     Loaded: loaded (/lib/systemd/system/nginx.service; enabled; preset: enabled)"
echo "     Active: active (running) since Mon 2024-03-17 10:00:00 UTC; 5h ago"
echo "   Main PID: 1235 (nginx)"
echo "      Tasks: 3 (limit: 4915)"
echo "     Memory: 8.5M"
echo "        CPU: 123ms"
echo "     CGroup: /system.slice/nginx.service"
echo ""
echo "--- Desglose ---"
echo "● / ○ / ×  = indicador visual (active/inactive/failed)"
echo "Loaded:     = ruta, enable state, preset"
echo "Active:     = active state (sub-state) since TIMESTAMP"
echo "Main PID:   = proceso principal"
echo "Tasks:      = threads del cgroup"
echo "Memory/CPU: = recursos consumidos"
echo "CGroup:     = jerarquia del cgroup"
echo ""
echo "--- Opciones utiles ---"
echo "  -l            lineas completas (sin truncar)"
echo "  --no-pager    sin less"
echo "  -n 50         ultimas 50 lineas de log"
echo "  -n 0          sin logs"
'
```

### Paso 1.2: systemctl show

```bash
docker compose exec debian-dev bash -c '
echo "=== systemctl show ==="
echo ""
echo "show devuelve TODAS las propiedades en formato key=value"
echo ""
echo "--- Propiedades utiles ---"
echo "  MainPID         PID del proceso principal"
echo "  ActiveState     active/inactive/failed"
echo "  SubState        running/exited/dead/listening"
echo "  Type            simple/forking/notify/oneshot"
echo "  Restart         on-failure/always/no"
echo "  RestartUSec     tiempo entre reinicios"
echo "  NRestarts       cuantas veces se ha reiniciado"
echo "  FragmentPath    ruta del archivo de unidad"
echo "  DropInPaths     rutas de drop-ins aplicados"
echo ""
echo "--- Ejemplos ---"
echo "systemctl show sshd --property=MainPID,ActiveState,Type"
echo "  MainPID=1234"
echo "  ActiveState=active"
echo "  Type=notify"
echo ""
echo "--- --value: solo el valor (util en scripts) ---"
echo "PID=\$(systemctl show sshd --property=MainPID --value)"
'
```

### Paso 1.3: systemctl cat

```bash
docker compose exec debian-dev bash -c '
echo "=== systemctl cat ==="
echo ""
echo "cat muestra el archivo de unidad con overrides aplicados"
echo "Indica el origen de cada parte:"
echo ""
echo "# /lib/systemd/system/nginx.service     ← original"
echo "[Unit]"
echo "Description=A high performance web server"
echo "..."
echo ""
echo "# /etc/systemd/system/nginx.service.d/override.conf  ← drop-in"
echo "[Service]"
echo "LimitNOFILE=65535"
echo ""
echo "--- Leer archivos directamente ---"
SVC=$(ls /usr/lib/systemd/system/systemd-journald.service 2>/dev/null | head -1)
if [[ -n "$SVC" ]]; then
    echo "Archivo: $SVC"
    echo ""
    cat "$SVC"
fi
'
```

---

## Parte 2 — Listar y filtrar

### Objetivo

Listar unidades instaladas y filtrar por tipo y estado.

### Paso 2.1: list-unit-files

```bash
docker compose exec debian-dev bash -c '
echo "=== list-unit-files ==="
echo ""
echo "Muestra TODAS las unidades instaladas (cargadas o no)"
echo "con su UnitFileState (enabled/disabled/static/masked)"
echo ""
echo "--- Por tipo ---"
echo "Servicios:"
ls /usr/lib/systemd/system/*.service 2>/dev/null | wc -l
echo ""
echo "Timers:"
ls /usr/lib/systemd/system/*.timer 2>/dev/null | wc -l
echo ""
echo "Sockets:"
ls /usr/lib/systemd/system/*.socket 2>/dev/null | wc -l
echo ""
echo "Targets:"
ls /usr/lib/systemd/system/*.target 2>/dev/null | wc -l
'
```

### Paso 2.2: Filtrar por estado

Antes de ejecutar, predice: habra mas servicios enabled o
static en el sistema?

```bash
docker compose exec debian-dev bash -c '
echo "=== Filtrar por UnitFileState ==="
echo ""
echo "--- Servicios con [Install] (enabled o disabled) ---"
WITH_INSTALL=0
WITHOUT_INSTALL=0
for svc in /usr/lib/systemd/system/*.service; do
    [[ -f "$svc" ]] || continue
    if grep -q "^\[Install\]" "$svc" 2>/dev/null; then
        ((WITH_INSTALL++))
    else
        ((WITHOUT_INSTALL++))
    fi
done
echo "Con [Install] (pueden ser enabled/disabled): $WITH_INSTALL"
echo "Sin [Install] (static):                      $WITHOUT_INSTALL"
echo ""
echo "--- Servicios enabled (symlinks en .wants/) ---"
ENABLED=$(find /etc/systemd/system -name "*.wants" -type d \
    -exec ls {} \; 2>/dev/null | grep "\.service$" | sort -u | wc -l)
echo "Servicios enabled: $ENABLED"
echo ""
echo "Los servicios static son mayoria en muchos sistemas"
echo "Son servicios internos de systemd que siempre se necesitan"
'
```

### Paso 2.3: Comparar Debian vs RHEL

```bash
docker compose exec debian-dev bash -c '
echo "=== Debian: servicios ==="
echo ""
echo "Total .service:"
ls /usr/lib/systemd/system/*.service 2>/dev/null | wc -l
echo ""
echo "Enabled (en .wants/):"
find /etc/systemd/system -name "*.wants" -type d \
    -exec ls {} \; 2>/dev/null | grep "\.service$" | sort -u | wc -l
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== AlmaLinux: servicios ==="
echo ""
echo "Total .service:"
ls /usr/lib/systemd/system/*.service 2>/dev/null | wc -l
echo ""
echo "Enabled (en .wants/):"
find /etc/systemd/system -name "*.wants" -type d \
    -exec ls {} \; 2>/dev/null | grep "\.service$" | sort -u | wc -l
'
```

---

## Parte 3 — Operaciones y scripts

### Objetivo

Entender las operaciones de gestion (enable/disable, mask) y
como usar systemctl en scripts.

### Paso 3.1: Operaciones basicas

```bash
docker compose exec debian-dev bash -c '
echo "=== Operaciones basicas de systemctl ==="
echo ""
echo "--- Ciclo de vida ---"
echo "  systemctl start SERVICE    arrancar"
echo "  systemctl stop SERVICE     detener"
echo "  systemctl restart SERVICE  stop + start"
echo "  systemctl reload SERVICE   recargar config sin reiniciar"
echo "  systemctl try-restart SVC  restart solo si esta corriendo"
echo "  systemctl reload-or-restart SVC  reload si puede, sino restart"
echo ""
echo "--- Boot ---"
echo "  systemctl enable SERVICE   arrancar al boot"
echo "  systemctl disable SERVICE  no arrancar al boot"
echo "  systemctl enable --now SVC enable + start"
echo "  systemctl disable --now SVC disable + stop"
echo ""
echo "--- Bloqueo ---"
echo "  systemctl mask SERVICE     bloquear completamente"
echo "  systemctl unmask SERVICE   desbloquear"
echo ""
echo "--- Que hace enable internamente ---"
echo "Lee [Install] WantedBy=multi-user.target"
echo "Crea symlink en /etc/systemd/system/multi-user.target.wants/"
echo ""
echo "--- Que hace mask internamente ---"
echo "Crea symlink /etc/systemd/system/SERVICE → /dev/null"
'
```

### Paso 3.2: daemon-reload

```bash
docker compose exec debian-dev bash -c '
echo "=== daemon-reload ==="
echo ""
echo "Despues de modificar archivos de unidad manualmente:"
echo "  systemctl daemon-reload"
echo ""
echo "Que hace:"
echo "  Re-lee TODOS los archivos de unidad"
echo "  NO reinicia servicios"
echo "  Solo recarga la configuracion"
echo ""
echo "Cuando hacer daemon-reload:"
echo "  - Despues de crear/editar en /etc/systemd/system/"
echo "  - Despues de crear/editar drop-ins manualmente"
echo "  - systemctl edit lo hace automaticamente"
echo ""
echo "Si olvidas daemon-reload, systemctl avisa:"
echo "  Warning: unit.service changed on disk."
echo "  Run systemctl daemon-reload to reload units."
echo ""
echo "Despues de daemon-reload, reiniciar el servicio:"
echo "  systemctl daemon-reload"
echo "  systemctl restart SERVICE"
'
```

### Paso 3.3: Verificaciones en scripts

```bash
docker compose exec debian-dev bash -c '
echo "=== Verificaciones en scripts ==="
echo ""
echo "--- Comandos silenciosos (solo exit code) ---"
echo "systemctl is-active --quiet SERVICE   # 0 si activo"
echo "systemctl is-enabled --quiet SERVICE  # 0 si enabled"
echo "systemctl is-failed --quiet SERVICE   # 0 si failed"
echo ""
echo "--- Patron en scripts ---"
cat << '\''SCRIPT'\''
#!/usr/bin/env bash
set -euo pipefail

check_service() {
    local svc="$1"
    printf "%-20s " "$svc"

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
}

for svc in sshd cron nginx; do
    check_service "$svc"
done
SCRIPT
echo ""
echo "--- Formato maquina (sin headers ni decoracion) ---"
echo "systemctl list-units --type=service --state=active --no-legend --plain"
echo "  --no-legend  sin headers"
echo "  --plain      sin formato de arbol"
'
```

### Paso 3.4: kill y senales

```bash
docker compose exec debian-dev bash -c '
echo "=== systemctl kill ==="
echo ""
echo "kill envia senales a TODOS los procesos del cgroup del servicio"
echo "(no solo al PID principal)"
echo ""
echo "--- kill vs stop ---"
echo "stop:  ejecuta ExecStop, respeta TimeoutStopSec, procedimiento gracil"
echo "kill:  envia la senal directamente a los procesos"
echo ""
echo "--- Opciones ---"
echo "systemctl kill SERVICE               # SIGTERM a todos"
echo "systemctl kill -s SIGHUP SERVICE     # reload"
echo "systemctl kill -s SIGKILL SERVICE    # kill forzado"
echo ""
echo "--- --kill-who ---"
echo "systemctl kill --kill-who=main SERVICE     # solo PID principal"
echo "systemctl kill --kill-who=control SERVICE  # solo procesos de control"
echo "systemctl kill --kill-who=all SERVICE      # todos (default)"
echo ""
echo "--- Operaciones del sistema ---"
echo "systemctl poweroff    = shutdown -h now"
echo "systemctl reboot      = shutdown -r now"
echo "systemctl suspend     = suspender a RAM"
echo "systemctl hibernate   = hibernar a disco"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. `systemctl status` muestra estado completo: load state, active
   state, PID, recursos, cgroup y logs recientes. Usar `-l -n 50
   --no-pager` para informacion completa.

2. `systemctl show` devuelve propiedades en formato key=value.
   `--property=X` filtra, `--value` muestra solo el valor.
   Util en scripts para extraer MainPID, ActiveState, Type.

3. `systemctl cat` muestra el archivo de unidad con la procedencia
   de cada seccion (original + drop-ins).

4. `enable` crea symlinks en `.wants/`, `disable` los elimina.
   `mask` crea symlink a `/dev/null` (bloqueo total).
   `--now` combina enable/disable con start/stop.

5. `daemon-reload` re-lee archivos de unidad modificados sin
   reiniciar servicios. Es necesario despues de editar archivos
   manualmente (systemctl edit lo hace automaticamente).

6. `is-active`, `is-enabled`, `is-failed` con `--quiet` retornan
   solo exit code (0=si), ideales para condicionales en scripts.
