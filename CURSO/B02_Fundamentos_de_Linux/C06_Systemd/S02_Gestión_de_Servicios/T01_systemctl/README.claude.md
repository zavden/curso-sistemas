# T01 — systemctl

## Erratas corregidas respecto a las fuentes originales

| # | Ubicación | Error | Corrección |
|---|-----------|-------|------------|
| 1 | max:24 | Texto en inglés: "reload configuration WITHOUT restarting the process" | Traducido a español |
| 2 | max:32,35,38,41-44 | Múltiples comentarios en inglés mezclados en contenido español | Traducidos a español |
| 3 | max:76 | "● bright = active (exited/finished successfully)" — no existe indicador "bright" | El indicador ● verde aplica tanto a `active (running)` como a `active (exited)`; no hay variante "bright" |
| 4 | max:94 | "Para Type=forking, es el PID del proceso padre que hizo fork()" | Para `Type=forking`, Main PID es el proceso **hijo** (el daemon que queda tras el fork), no el padre |
| 5 | max:344-346 | Opciones `--kill-who=` sin el comando completo `systemctl kill` | Agregado el comando completo |
| 6 | max:404 | `Result` incluye valor "resources" que no existe | Valores reales: `success`, `exit-code`, `signal`, `timeout`, `core-dump`, `watchdog`, `start-limit-hit`, `oom-kill` |

---

## systemctl — Centro de control de systemd

`systemctl` es la herramienta CLI principal para interactuar con systemd (PID 1). Gestiona servicios, targets, sockets, timers y todas las unidades del sistema:

```
systemctl COMANDO UNIDAD.TIPO

systemctl start nginx.service
systemctl stop nginx
systemctl status nginx.service
systemctl enable nginx
```

## Operaciones básicas con servicios

```bash
# Arrancar un servicio:
sudo systemctl start nginx.service

# Detener un servicio:
sudo systemctl stop nginx.service

# Reiniciar (stop + start):
sudo systemctl restart nginx.service

# Recargar configuración SIN reiniciar el proceso:
sudo systemctl reload nginx.service
# Solo funciona si el servicio tiene ExecReload definido
# Envía SIGHUP al proceso principal por defecto

# Reload si soporta, restart si no:
sudo systemctl reload-or-restart nginx.service
# Preferido en scripts — es seguro sin saber si el servicio soporta reload

# Reiniciar solo si ya está corriendo, no hacer nada si no:
sudo systemctl try-restart nginx.service

# Verificar si está corriendo (exit code 0 = sí):
systemctl is-active nginx.service

# Verificar si arranca al boot:
systemctl is-enabled nginx.service
```

### El sufijo .service

```bash
# El sufijo .service es opcional — systemd lo asume por defecto:
systemctl start nginx       # equivalente a nginx.service

# PERO puede haber ambigüedad con targets:
systemctl start network     # ¿network.service o network.target?
# Buena práctica: incluir siempre .service para claridad
```

## status — Inspección detallada

```bash
systemctl status nginx.service
```

```
● nginx.service - A high performance web server and a reverse proxy server
     Loaded: loaded (/lib/systemd/system/nginx.service; enabled; preset: enabled)
     Active: active (running) since Mon 2024-03-17 10:00:00 UTC; 5h 23min ago
       Docs: man:nginx(8)
    Process: 1234 ExecStartPre=/usr/sbin/nginx -t -q -g daemon on; ... (code=exited, status=0/SUCCESS)
   Main PID: 1235 (nginx)
      Tasks: 3 (limit: 4915)
     Memory: 8.5M
        CPU: 123ms
     CGroup: /system.slice/nginx.service
             ├─1235 "nginx: master process /usr/sbin/nginx ..."
             ├─1236 "nginx: worker process"
             └─1237 "nginx: worker process"

Mar 17 10:00:00 server systemd[1]: Starting nginx.service - A high performance web server...
Mar 17 10:00:00 server systemd[1]: Started nginx.service - A high performance web server...
```

### Desglose línea por línea

```
● / ○ / × — Indicador visual:
  ● (verde) = active (running o exited)
  ○ (blanco) = inactive (dead)
  × (rojo) = failed

Loaded: loaded (RUTA; ENABLE_STATE; PRESET)
  RUTA         → dónde está el archivo de unidad
  ENABLE_STATE → enabled | disabled | masked | static
  PRESET       → enabled | disabled (del preset del sistema)

Active: ACTIVE_STATE (SUB_STATE) since TIMESTAMP; DURACIÓN ago
  SUB_STATE depende del Type= de la unidad:
    running   → proceso principal corriendo
    exited    → Type=oneshot, ExecStart terminó OK
    listening → socket escuchando
    waiting   → esperando (idle, timers, etc.)

Main PID: 1235 (nginx)
  → El PID que systemd monitorea como "el" proceso del servicio
  → Para Type=forking, es el proceso hijo (el daemon que queda
    después de que el padre hace fork() y termina)

Tasks: 3 (limit: 4915)
  → Número de tasks en el cgroup (procesos + threads)
  → limit: máximo configurado en la unidad

Memory: 8.5M / CPU: 123ms
  → Recursos consumidos por el cgroup del servicio

CGroup: /system.slice/nginx.service
  → Jerarquía del cgroup v2
  → Todos los procesos del servicio se listan aquí

Las últimas líneas son logs recientes del journal
```

### Opciones de status

```bash
# Sin pager (para scripts, CI, Docker):
systemctl status nginx --no-pager

# Más o menos líneas de log:
systemctl status nginx -n 100     # últimas 100 líneas
systemctl status nginx -n 0       # sin logs

# Líneas completas (sin truncar rutas):
systemctl status nginx -l

# Todo junto:
systemctl status nginx -l --no-pager -n 50

# Status de múltiples servicios:
systemctl status nginx cron sshd
```

## enable / disable — Arranque al boot

```bash
# Habilitar (arranca automáticamente al boot):
sudo systemctl enable nginx.service
# Created symlink /etc/systemd/system/multi-user.target.wants/nginx.service
#  → /lib/systemd/system/nginx.service

# Deshabilitar (NO arranca al boot):
sudo systemctl disable nginx.service
# Removed /etc/systemd/system/multi-user.target.wants/nginx.service

# Habilitar Y arrancar inmediatamente:
sudo systemctl enable --now nginx.service
# Equivale a: enable + start

# Deshabilitar Y detener inmediatamente:
sudo systemctl disable --now nginx.service
# Equivale a: disable + stop

# Verificar si está habilitado:
systemctl is-enabled nginx.service
# enabled / disabled / static / masked
```

### Qué hace enable internamente

```bash
# 1. systemctl lee la sección [Install] de la unidad:
#    WantedBy=multi-user.target
#
# 2. Crea un symlink:
#    /etc/systemd/system/multi-user.target.wants/nginx.service
#    → /lib/systemd/system/nginx.service
#
# 3. Al arrancar, multi-user.target activa todo en su .wants/

# Si no hay sección [Install], la unidad es "static" y no se puede enable:
systemctl enable systemd-journald.service
# The unit files have no installation config (WantedBy, RequiredBy, etc.)
```

## mask / unmask — Bloqueo total

```bash
# Enmascarar (bloquear completamente — symlink a /dev/null):
sudo systemctl mask nginx.service
# Created symlink /etc/systemd/system/nginx.service → /dev/null

# Intentar arrancar un servicio enmascarado:
sudo systemctl start nginx.service
# Failed to start nginx.service: Unit nginx.service is masked.

# Desenmascarar:
sudo systemctl unmask nginx.service
# Removed /etc/systemd/system/nginx.service
```

```
┌──────────────────────────────────────────────────────────────┐
│  disable vs mask                                             │
├──────────────────────────────────────────────────────────────┤
│  disable:                                                    │
│    • Elimina symlink en target.wants/                        │
│    • No arranca al boot                                      │
│    • systemctl start → FUNCIONA                              │
│                                                              │
│  mask:                                                       │
│    • Crea symlink a /dev/null                                │
│    • No arranca al boot                                      │
│    • systemctl start → ERROR "Unit is masked"                │
│    • No puede ser iniciado NI como dependencia               │
└──────────────────────────────────────────────────────────────┘
```

## list-units — Listar unidades en memoria

```bash
# Todas las unidades cargadas y activas:
systemctl list-units

# Filtrar por tipo:
systemctl list-units --type=service
systemctl list-units --type=timer
systemctl list-units --type=socket
systemctl list-units --type=mount

# Filtrar por estado:
systemctl list-units --state=active
systemctl list-units --state=failed
systemctl list-units --state=inactive

# Combinado:
systemctl list-units --type=service --state=active

# Incluir inactivas cargadas:
systemctl list-units --all --type=service

# Formato máquina (sin headers, sin colores):
systemctl list-units --type=service --no-legend --plain --no-pager

# Solo las fallidas:
systemctl --failed
# Equivalente a: systemctl list-units --state=failed

# Contar servicios activos:
systemctl list-units --type=service --state=active --no-legend | wc -l
```

## list-unit-files — Unidades instaladas

```bash
# Todas las unit files instaladas (enabled, disabled, static, masked):
systemctl list-unit-files
systemctl list-unit-files --type=service

# Filtrar por estado:
systemctl list-unit-files --state=enabled
systemctl list-unit-files --state=disabled
systemctl list-unit-files --state=masked
```

`list-units` vs `list-unit-files`:
- `list-units` — muestra unidades **cargadas en memoria** (activas o recientemente activas)
- `list-unit-files` — muestra **todos los archivos** instalados en disco, incluso los no cargados

## show — Propiedades de una unidad

```bash
# Ver TODAS las propiedades (formato key=value):
systemctl show nginx.service

# Propiedades específicas:
systemctl show nginx.service --property=MainPID
# MainPID=1235

# Múltiples propiedades:
systemctl show nginx --property=ActiveState,SubState,MainPID,Type
# ActiveState=active
# SubState=running
# MainPID=1235
# Type=forking

# --value: solo el valor (sin el "MainPID="):
PID=$(systemctl show nginx --property=MainPID --value)

# Útil en scripts:
STATE=$(systemctl show nginx --property=ActiveState --value)
if [[ "$STATE" != "active" ]]; then
    echo "nginx no está activo"
fi
```

### Propiedades útiles para scripting

```bash
# Estado:
systemctl show nginx --property=ActiveState     # active/inactive/failed
systemctl show nginx --property=SubState         # running/exited/dead/listening

# PID:
systemctl show nginx --property=MainPID          # PID del proceso principal
systemctl show nginx --property=ControlPID       # PID del proceso de control

# Resultado de última ejecución:
systemctl show nginx --property=Result
# success | exit-code | signal | timeout | core-dump
# watchdog | start-limit-hit | oom-kill

# Configuración:
systemctl show nginx --property=ExecStart
systemctl show nginx --property=ExecReload
systemctl show nginx --property=FragmentPath     # ruta del archivo de unidad
systemctl show nginx --property=DropInPaths      # rutas de drop-ins aplicados

# Recursos:
systemctl show nginx --property=MemoryCurrent,CPUUsageNSec

# Reinicios:
systemctl show nginx --property=Restart          # política (on-failure/always/no)
systemctl show nginx --property=NRestarts        # cuántas veces se ha reiniciado

# Tiempo:
systemctl show nginx --property=ActiveEnterTimestamp
systemctl show nginx --property=InactiveExitTimestamp
```

## cat — Ver el archivo de unidad

```bash
# Muestra el archivo de unidad ORIGINAL + overrides aplicados:
systemctl cat nginx.service

# Salida ejemplo:
# # /usr/lib/systemd/system/nginx.service     ← original
# [Unit]
# Description=A high performance web server...
# ...
#
# # /etc/systemd/system/nginx.service.d/override.conf  ← drop-in
# [Service]
# LimitNOFILE=65535

# Muestra el origen de cada parte (archivo original + drop-ins)
```

## edit — Modificar unidades

```bash
# Crear un drop-in (override parcial):
sudo systemctl edit nginx.service
# Abre el editor con un archivo vacío
# Al guardar: crea /etc/systemd/system/nginx.service.d/override.conf
# Ejecuta daemon-reload automáticamente

# Override completo (copia entera a /etc/):
sudo systemctl edit --full nginx.service
# Copia la unidad a /etc/systemd/system/ y abre el editor
# ⚠️ Pierde updates futuros del paquete

# Especificar el editor:
sudo EDITOR=vim systemctl edit nginx.service
sudo SYSTEMD_EDITOR=vim systemctl edit nginx.service
```

## daemon-reload

```bash
# Recargar archivos de unidad después de modificarlos manualmente:
sudo systemctl daemon-reload
# Re-escanea /usr/lib/, /etc/, /run/
# Actualiza el estado de systemd en memoria
# NO reinicia servicios activos

# ¿Cuándo hacer daemon-reload?
# - Después de crear/editar archivos .service en /etc/systemd/system/
# - Después de modificar drop-ins manualmente
# - systemctl edit lo hace automáticamente

# Si olvidas, systemctl te avisa:
# Warning: nginx.service changed on disk. Run 'systemctl daemon-reload'.

# Después de daemon-reload, reiniciar el servicio para aplicar cambios:
sudo systemctl daemon-reload
sudo systemctl restart nginx.service
```

## kill — Enviar señales

```bash
# Enviar señal a TODOS los procesos del cgroup del servicio:
sudo systemctl kill nginx.service
# Envía SIGTERM a todos los procesos del cgroup

# Señal específica:
sudo systemctl kill -s SIGHUP nginx.service    # reload de config
sudo systemctl kill -s SIGKILL nginx.service   # kill forzado
sudo systemctl kill -s SIGUSR1 nginx.service   # señal custom

# --kill-who: a quién enviar la señal
sudo systemctl kill --kill-who=main nginx.service      # solo PID principal
sudo systemctl kill --kill-who=control nginx.service   # solo procesos de control
sudo systemctl kill --kill-who=all nginx.service       # todos (default)
```

```
kill vs stop:
  stop  → Ejecuta ExecStop → SIGTERM al proceso → espera TimeoutStopSec
          Procedimiento grácil definido por la unidad
  kill  → Envía señal DIRECTA a los procesos (no pasa por ExecStop)
          Útil cuando un servicio no responde a stop
```

## list-dependencies — Ver dependencias

```bash
# Ver qué unidades necesita un servicio:
systemctl list-dependencies nginx.service

# Ver qué unidades dependen de un servicio (reversa):
systemctl list-dependencies --reverse nginx.service

# Ver el árbol completo de un target:
systemctl list-dependencies multi-user.target

# Sin formato de árbol (lista plana):
systemctl list-dependencies --plain multi-user.target
```

## isolate — Cambiar de target

```bash
# Cambiar a otro target (detener servicios no requeridos por el nuevo target):
sudo systemctl isolate multi-user.target
sudo systemctl isolate graphical.target

# isolate detiene todos los servicios que NO son dependencia
# del target elegido — equivalente conceptual a cambiar de runlevel
```

## Operaciones del sistema

```bash
# Apagar:
sudo systemctl poweroff          # equivalente a: shutdown -h now

# Reiniciar:
sudo systemctl reboot            # equivalente a: shutdown -r now

# Solo detener (sin apagar hardware):
sudo systemctl halt

# Suspender (a RAM):
sudo systemctl suspend

# Hibernar (a disco):
sudo systemctl hibernate

# Suspender y luego hibernar:
sudo systemctl suspend-then-hibernate

# Hybrid sleep (suspender + copiar a disco):
sudo systemctl hybrid-sleep
```

## Operaciones en scripts

```bash
# Verificaciones silenciosas (solo exit code, sin salida):
systemctl is-active --quiet nginx    # 0 si activo
systemctl is-enabled --quiet nginx   # 0 si habilitado
systemctl is-failed --quiet nginx    # 0 si está en estado failed

# Patrón condicional:
if ! systemctl is-active --quiet nginx; then
    echo "nginx no está activo, arrancando..."
    sudo systemctl start nginx
fi

# Nota sobre Type y start:
# Si el servicio es Type=notify, start espera a que notifique readiness
# Si es Type=simple, start retorna inmediatamente (no garantiza que esté listo)

# Formato máquina para parsear:
systemctl list-units --type=service --state=active --no-legend --plain
# --no-legend: sin headers
# --plain: sin formato de árbol
```

### Script de verificación de servicios

```bash
#!/usr/bin/env bash
set -euo pipefail

check_service() {
    local svc="$1"
    printf "%-25s " "$svc"

    if systemctl is-active --quiet "$svc" 2>/dev/null; then
        printf "ACTIVO   "
    else
        printf "PARADO   "
    fi

    if systemctl is-enabled --quiet "$svc" 2>/dev/null; then
        echo "enabled"
    else
        echo "disabled"
    fi
}

SERVICES=(sshd cron systemd-logind systemd-journald)

for svc in "${SERVICES[@]}"; do
    check_service "$svc"
done
```

---

## Lab — systemctl

### Objetivo

Dominar systemctl como herramienta central de gestión de systemd:
inspeccionar servicios con status/show/cat, listar y filtrar
unidades por tipo y estado, entender enable/disable vs mask/unmask,
y usar verificaciones silenciosas en scripts.

### Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

### Parte 1 — Inspección de servicios

#### Paso 1.1: systemctl status

```bash
docker compose exec debian-dev bash -c '
echo "=== systemctl status ==="
echo ""
echo "status muestra información completa de un servicio:"
echo ""
echo "Ejemplo de salida típica:"
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
echo "CGroup:     = jerarquía del cgroup"
echo ""
echo "--- Opciones útiles ---"
echo "  -l            líneas completas (sin truncar)"
echo "  --no-pager    sin less"
echo "  -n 50         últimas 50 líneas de log"
echo "  -n 0          sin logs"
'
```

#### Paso 1.2: systemctl show

```bash
docker compose exec debian-dev bash -c '
echo "=== systemctl show ==="
echo ""
echo "show devuelve TODAS las propiedades en formato key=value"
echo ""
echo "--- Propiedades útiles ---"
echo "  MainPID         PID del proceso principal"
echo "  ActiveState     active/inactive/failed"
echo "  SubState        running/exited/dead/listening"
echo "  Type            simple/forking/notify/oneshot"
echo "  Restart         on-failure/always/no"
echo "  RestartUSec     tiempo entre reinicios"
echo "  NRestarts       cuántas veces se ha reiniciado"
echo "  FragmentPath    ruta del archivo de unidad"
echo "  DropInPaths     rutas de drop-ins aplicados"
echo ""
echo "--- Ejemplos ---"
echo "systemctl show sshd --property=MainPID,ActiveState,Type"
echo "  MainPID=1234"
echo "  ActiveState=active"
echo "  Type=notify"
echo ""
echo "--- --value: solo el valor (útil en scripts) ---"
echo "PID=\$(systemctl show sshd --property=MainPID --value)"
'
```

#### Paso 1.3: systemctl cat

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

### Parte 2 — Listar y filtrar

#### Paso 2.1: list-unit-files

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

#### Paso 2.2: Filtrar por estado

Antes de ejecutar, predice: ¿habrá más servicios enabled o static en el sistema?

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
echo "Los servicios static son mayoría en muchos sistemas"
echo "Son servicios internos de systemd que siempre se necesitan"
'
```

#### Paso 2.3: Comparar Debian vs RHEL

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

### Parte 3 — Operaciones y scripts

#### Paso 3.1: Operaciones básicas

```bash
docker compose exec debian-dev bash -c '
echo "=== Operaciones básicas de systemctl ==="
echo ""
echo "--- Ciclo de vida ---"
echo "  systemctl start SERVICE    arrancar"
echo "  systemctl stop SERVICE     detener"
echo "  systemctl restart SERVICE  stop + start"
echo "  systemctl reload SERVICE   recargar config sin reiniciar"
echo "  systemctl try-restart SVC  restart solo si está corriendo"
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
echo "--- Qué hace enable internamente ---"
echo "Lee [Install] WantedBy=multi-user.target"
echo "Crea symlink en /etc/systemd/system/multi-user.target.wants/"
echo ""
echo "--- Qué hace mask internamente ---"
echo "Crea symlink /etc/systemd/system/SERVICE → /dev/null"
'
```

#### Paso 3.2: daemon-reload

```bash
docker compose exec debian-dev bash -c '
echo "=== daemon-reload ==="
echo ""
echo "Después de modificar archivos de unidad manualmente:"
echo "  systemctl daemon-reload"
echo ""
echo "Qué hace:"
echo "  Re-lee TODOS los archivos de unidad"
echo "  NO reinicia servicios"
echo "  Solo recarga la configuración"
echo ""
echo "Cuándo hacer daemon-reload:"
echo "  - Después de crear/editar en /etc/systemd/system/"
echo "  - Después de crear/editar drop-ins manualmente"
echo "  - systemctl edit lo hace automáticamente"
echo ""
echo "Si olvidas daemon-reload, systemctl avisa:"
echo "  Warning: unit.service changed on disk."
echo "  Run systemctl daemon-reload to reload units."
echo ""
echo "Después de daemon-reload, reiniciar el servicio:"
echo "  systemctl daemon-reload"
echo "  systemctl restart SERVICE"
'
```

#### Paso 3.3: Verificaciones en scripts

```bash
docker compose exec debian-dev bash -c '
echo "=== Verificaciones en scripts ==="
echo ""
echo "--- Comandos silenciosos (solo exit code) ---"
echo "systemctl is-active --quiet SERVICE   # 0 si activo"
echo "systemctl is-enabled --quiet SERVICE  # 0 si enabled"
echo "systemctl is-failed --quiet SERVICE   # 0 si failed"
echo ""
echo "--- Patrón en scripts ---"
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
echo "--- Formato máquina (sin headers ni decoración) ---"
echo "systemctl list-units --type=service --state=active --no-legend --plain"
echo "  --no-legend  sin headers"
echo "  --plain      sin formato de árbol"
'
```

#### Paso 3.4: kill y señales

```bash
docker compose exec debian-dev bash -c '
echo "=== systemctl kill ==="
echo ""
echo "kill envía señales a TODOS los procesos del cgroup del servicio"
echo "(no solo al PID principal)"
echo ""
echo "--- kill vs stop ---"
echo "stop:  ejecuta ExecStop, respeta TimeoutStopSec, procedimiento grácil"
echo "kill:  envía la señal directamente a los procesos"
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

### Conceptos reforzados

1. `systemctl status` muestra estado completo: load state, active state, PID, recursos, cgroup y logs recientes. Usar `-l -n 50 --no-pager` para información completa.

2. `systemctl show` devuelve propiedades en formato key=value. `--property=X` filtra, `--value` muestra solo el valor. Útil en scripts para extraer MainPID, ActiveState, Type.

3. `systemctl cat` muestra el archivo de unidad con la procedencia de cada sección (original + drop-ins).

4. `enable` crea symlinks en `.wants/`, `disable` los elimina. `mask` crea symlink a `/dev/null` (bloqueo total). `--now` combina enable/disable con start/stop.

5. `daemon-reload` re-lee archivos de unidad modificados sin reiniciar servicios. Es necesario después de editar archivos manualmente (`systemctl edit` lo hace automáticamente).

6. `is-active`, `is-enabled`, `is-failed` con `--quiet` retornan solo exit code (0=sí), ideales para condicionales en scripts.

---

## Ejercicios

### Ejercicio 1 — Anatomía de status

Ejecuta `systemctl status` en un servicio activo de tu sistema (por ejemplo `sshd` o `systemd-journald`). Para cada línea de la salida, identifica:
- ¿Qué indicador visual aparece y qué significa?
- ¿Cuál es la ruta del archivo de unidad?
- ¿Cuál es el `ENABLE_STATE` y qué indica?
- ¿Cuál es el `Main PID` y cuántos tasks tiene?

```bash
systemctl status sshd -l --no-pager -n 5
```

<details><summary>Predicción</summary>

La salida muestra:
- `●` verde → servicio activo
- `Loaded: loaded (/usr/lib/systemd/system/sshd.service; enabled; ...)` → la ruta del unit file, enabled = arranca al boot
- `Active: active (running) since ...` → running = proceso principal vivo
- `Main PID: XXXX (sshd)` → el PID del daemon sshd
- `Tasks: N` → número de procesos/threads en su cgroup (al menos 1 por el master + 1 por cada sesión activa)
- Las últimas líneas son logs del journal

</details>

### Ejercicio 2 — show vs status

Usando `systemctl show`, extrae las siguientes propiedades de `sshd` (o un servicio activo): `MainPID`, `ActiveState`, `SubState`, `Type`, `FragmentPath`. Luego usa `--value` para obtener solo el valor de `MainPID`.

¿Qué diferencia hay entre la información de `show` y la de `status`?

```bash
systemctl show sshd --property=MainPID,ActiveState,SubState,Type,FragmentPath
echo "---"
systemctl show sshd --property=MainPID --value
```

<details><summary>Predicción</summary>

`show` devuelve propiedades en formato `key=value` parseable por scripts. `status` es para humanos, con formato visual, indicadores de color y logs.

Ejemplo de salida de `show`:
```
MainPID=1234
ActiveState=active
SubState=running
Type=notify
FragmentPath=/usr/lib/systemd/system/sshd.service
```

Con `--value` solo se obtiene `1234` (sin el prefijo `MainPID=`), lo que facilita asignar a una variable en un script.

</details>

### Ejercicio 3 — cat y drop-ins

Usa `systemctl cat` en un servicio del sistema. Observa si tiene drop-ins aplicados. Luego compara con leer el archivo directamente usando `cat` en la ruta del `FragmentPath`.

```bash
systemctl cat systemd-journald.service
echo "==="
cat "$(systemctl show systemd-journald --property=FragmentPath --value)"
```

<details><summary>Predicción</summary>

`systemctl cat` muestra el contenido del archivo original **más** cualquier drop-in override, con comentarios indicando la procedencia de cada parte (ej: `# /usr/lib/systemd/system/...` y `# /etc/systemd/system/.../override.conf`).

`cat` directo solo muestra el archivo original sin drop-ins. Si no hay drop-ins, ambas salidas son idénticas excepto por el comentario de procedencia que agrega `systemctl cat`.

</details>

### Ejercicio 4 — list-units vs list-unit-files

Ejecuta ambos comandos filtrados por `--type=service` y compara los conteos. ¿Por qué son diferentes?

```bash
echo "Unidades cargadas en memoria:"
systemctl list-units --type=service --no-legend | wc -l
echo ""
echo "Archivos de unidad instalados en disco:"
systemctl list-unit-files --type=service --no-legend | wc -l
```

<details><summary>Predicción</summary>

`list-unit-files` muestra **más** entradas que `list-units` porque lista todos los archivos `.service` instalados en disco, incluyendo servicios que no están cargados en memoria.

`list-units` (sin `--all`) solo muestra unidades activas o que fueron activadas recientemente. Un servicio que está `disabled` y nunca fue iniciado no aparece en `list-units` pero sí en `list-unit-files`.

Agregar `--all` a `list-units` muestra también las inactivas cargadas, pero aun así puede diferir de `list-unit-files` porque hay archivos en disco que systemd ni siquiera ha cargado.

</details>

### Ejercicio 5 — Filtrar unidades fallidas

Encuentra todos los servicios en estado `failed` y, para cada uno, investiga la causa consultando la propiedad `Result`.

```bash
# Listar fallidos:
systemctl --failed --no-pager

# Para un servicio fallido (reemplaza SERVICIO):
# systemctl show SERVICIO --property=Result,ExecMainCode,ExecMainStatus --value
```

<details><summary>Predicción</summary>

`systemctl --failed` lista los servicios cuyo `ActiveState=failed`. En un sistema sano, puede no haber ninguno.

La propiedad `Result` indica la causa:
- `exit-code` → el proceso terminó con un código de salida no-cero
- `signal` → terminó por una señal (ej: SIGSEGV, SIGKILL)
- `timeout` → no respondió dentro de `TimeoutStartSec`/`TimeoutStopSec`
- `start-limit-hit` → se reinició demasiadas veces en poco tiempo
- `core-dump` → generó un core dump

`ExecMainCode` y `ExecMainStatus` dan el detalle numérico del código de salida o señal.

</details>

### Ejercicio 6 — enable, disable, is-enabled

Para un servicio que puedas manipular (ej: `cron`, `crond`), verifica su estado con `is-enabled`, luego observa qué symlink existe en `/etc/systemd/system/*.wants/`. Ejecuta `disable` y verifica que el symlink desapareció. Luego `enable` de nuevo.

```bash
# Verificar estado actual:
systemctl is-enabled cron

# Ver el symlink:
ls -la /etc/systemd/system/multi-user.target.wants/ | grep cron

# Disable:
sudo systemctl disable cron
systemctl is-enabled cron
ls -la /etc/systemd/system/multi-user.target.wants/ | grep cron

# Re-enable:
sudo systemctl enable cron
systemctl is-enabled cron
```

<details><summary>Predicción</summary>

- `is-enabled cron` devuelve `enabled` y exit code 0
- En `.wants/` hay un symlink `cron.service → /lib/systemd/system/cron.service`
- Después de `disable`: `is-enabled` devuelve `disabled`, el symlink desaparece
- `disable` no detiene el servicio — solo evita que arranque al boot
- Después de `enable`: el symlink se re-crea y vuelve a `enabled`
- El servicio sigue corriendo todo el tiempo (disable/enable no afectan el estado activo)

</details>

### Ejercicio 7 — disable vs mask

¿Cuál es la diferencia práctica entre `disable` y `mask`? Para verificar:
1. `disable` un servicio y luego intenta `start` manualmente
2. `mask` un servicio y luego intenta `start` manualmente

```bash
# Con disable:
sudo systemctl disable cron
sudo systemctl start cron          # ¿funciona?
systemctl is-active cron

# Con mask:
sudo systemctl stop cron
sudo systemctl mask cron
sudo systemctl start cron          # ¿funciona?

# Limpiar:
sudo systemctl unmask cron
sudo systemctl enable --now cron
```

<details><summary>Predicción</summary>

- Después de `disable` + `start`: **funciona**. El servicio arranca normalmente. `disable` solo previene el arranque automático al boot, pero permite inicio manual.
- Después de `mask` + `start`: **falla** con error `"Unit cron.service is masked"`. `mask` crea un symlink a `/dev/null` que bloquea cualquier intento de inicio, manual o por dependencia.
- `unmask` elimina el symlink a `/dev/null` y restaura el comportamiento normal
- `enable --now` re-crea el symlink en `.wants/` y además arranca el servicio

</details>

### Ejercicio 8 — kill vs stop

Investiga la diferencia entre `systemctl stop` y `systemctl kill` para un servicio. Consulta la propiedad `ExecStop` para entender qué hace `stop` internamente.

```bash
# Ver qué ejecuta stop:
systemctl show nginx --property=ExecStop

# Ver los procesos del cgroup:
systemctl status nginx --no-pager -n 0

# Comparar:
# sudo systemctl stop nginx    → ejecuta ExecStop, espera TimeoutStopSec
# sudo systemctl kill nginx    → envía SIGTERM directo a todos los procesos del cgroup
```

<details><summary>Predicción</summary>

`stop` sigue un procedimiento ordenado:
1. Ejecuta el comando definido en `ExecStop` (si existe)
2. Envía SIGTERM al proceso principal
3. Espera `TimeoutStopSec` (por defecto 90s)
4. Si no ha terminado, envía SIGKILL

`kill` omite `ExecStop` y envía la señal directamente a todos los procesos del cgroup. Por defecto envía SIGTERM, pero se puede cambiar con `-s`.

`kill` es útil cuando un servicio se queda colgado y `stop` no funciona. Pero `stop` es preferible en operación normal porque permite al servicio cerrar conexiones, guardar estado, etc.

</details>

### Ejercicio 9 — Script de monitoreo

Escribe un script que reciba una lista de servicios como argumentos y para cada uno muestre: nombre, estado activo, si está habilitado, y el PID principal. Usa `systemctl show` con `--property` y `--value`.

```bash
#!/usr/bin/env bash
set -euo pipefail

if [[ $# -eq 0 ]]; then
    echo "Uso: $0 servicio1 [servicio2 ...]" >&2
    exit 1
fi

printf "%-25s %-12s %-10s %s\n" "SERVICIO" "ESTADO" "BOOT" "PID"
printf "%-25s %-12s %-10s %s\n" "--------" "------" "----" "---"

for svc in "$@"; do
    state=$(systemctl show "$svc" --property=ActiveState --value 2>/dev/null)
    enabled=$(systemctl is-enabled "$svc" 2>/dev/null || true)
    pid=$(systemctl show "$svc" --property=MainPID --value 2>/dev/null)

    printf "%-25s %-12s %-10s %s\n" "$svc" "$state" "$enabled" "$pid"
done
```

<details><summary>Predicción</summary>

Ejemplo de salida para `./monitor.sh sshd cron nginx`:
```
SERVICIO                  ESTADO       BOOT       PID
--------                  ------       ----       ---
sshd                      active       enabled    1234
cron                      active       enabled    567
nginx                     inactive     disabled   0
```

- `ActiveState` devuelve `active`, `inactive`, o `failed`
- `is-enabled` devuelve `enabled`, `disabled`, `static`, o `masked`
- `MainPID=0` significa que el servicio no tiene proceso corriendo
- `2>/dev/null` suprime errores si un servicio no existe
- `|| true` evita que `set -e` termine el script si `is-enabled` retorna exit code no-cero (lo hace cuando el servicio está disabled)

</details>

### Ejercicio 10 — Resumen de propiedades

Para un servicio activo, usa `systemctl show` para responder estas preguntas sin usar `status`:
1. ¿El servicio está activo? (`ActiveState`)
2. ¿Cuál es su tipo? (`Type`)
3. ¿Cuántas veces se ha reiniciado? (`NRestarts`)
4. ¿Dónde está su archivo de unidad? (`FragmentPath`)
5. ¿Cuál es su política de reinicio? (`Restart`)
6. ¿Cuánta memoria usa? (`MemoryCurrent`)
7. ¿Cuál fue el resultado de su última ejecución? (`Result`)

```bash
systemctl show sshd --property=ActiveState,Type,NRestarts,FragmentPath,Restart,MemoryCurrent,Result
```

<details><summary>Predicción</summary>

Ejemplo de salida:
```
ActiveState=active
Type=notify
NRestarts=0
FragmentPath=/usr/lib/systemd/system/sshd.service
Restart=on-failure
MemoryCurrent=5242880
Result=success
```

- `Type=notify` → sshd usa sd_notify() para indicar que está listo
- `NRestarts=0` → no se ha reiniciado automáticamente
- `Restart=on-failure` → se reinicia solo si termina con error
- `MemoryCurrent` → en bytes (5242880 = 5 MB)
- `Result=success` → la última ejecución fue exitosa

`show` es la herramienta para scripting porque devuelve datos estructurados en `key=value`, a diferencia de `status` que es para consumo humano con formato visual.

</details>
